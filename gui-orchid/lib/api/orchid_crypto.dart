import 'dart:math';
import 'dart:typed_data';
import 'package:ethereum_address/ethereum_address.dart';
import 'package:flutter/material.dart';
import 'package:orchid/api/orchid_eth/eth_transaction.dart';
import 'package:orchid/api/preferences/user_preferences.dart';
import 'package:orchid/util/hex.dart';
import 'package:pointycastle/api.dart';
import 'package:pointycastle/digests/sha3.dart';
import 'package:pointycastle/ecc/api.dart';
import 'package:pointycastle/ecc/curves/secp256k1.dart';
import 'package:pointycastle/key_generators/api.dart';
import 'package:pointycastle/key_generators/ec_key_generator.dart';
import 'package:pointycastle/src/utils.dart' as decode;
import 'package:uuid/uuid.dart';
import 'package:uuid/uuid_util.dart';
import 'package:web3dart/credentials.dart';
import 'package:web3dart/crypto.dart';
import 'package:convert/convert.dart';

class Crypto {
  static final ECDomainParameters curve = ECCurve_secp256k1();

  /// Generate an Ethereum KeyPair and corresponding address.
  static EthereumKeyPair generateKeyPair() {
    // Generate a keypair using Dart's secure random source.
    final generator = ECKeyGenerator();
    final params = ECKeyGeneratorParameters(curve);
    generator.init(ParametersWithRandom(params, DartSecureRandom()));
    final key = generator.generateKeyPair();
    final BigInt privateKey = (key.privateKey as ECPrivateKey).d;
    return fromPrivateKey(privateKey);
  }

  static EthereumKeyPair fromPrivateKey(BigInt privateKey) {
    final ECDomainParameters curve = ECCurve_secp256k1();

    final ECPoint publicKeyPoint = curve.G * privateKey; // EC scalar multiply

    // X9.62 encoded uncompressed ECPoint is just the prefix value '4' followed by x, y.
    final encoded = publicKeyPoint.getEncoded(false).buffer;
    // Remove the prefix byte
    final publicKey = Uint8List.view(encoded, 1);

    // Derive the Ethereum address from the public key
    final SHA3Digest sha3digest = SHA3Digest(256 /*bits*/);
    final hashed = sha3digest.process(publicKey);
    final ethereumAddress = Uint8List.view(
        hashed.buffer, 32 /*bytes*/ - 20 /*eth address length bytes*/);

    return new EthereumKeyPair(
        private: privateKey,
        public: toHex(publicKey),
        addressString: toHex(ethereumAddress));
  }

  static String toHex(Uint8List bytes) {
    var result = new StringBuffer('0x');
    bytes.forEach((val) {
      var pad = val < 16 ? '0' : '';
      result.write('$pad${val.toRadixString(16)}');
    });
    return result.toString();
  }

  /// Parse the string with an optional 0x prefix and return the BigInt or
  /// throw an exception if invalid.
  static BigInt parseEthereumPrivateKey(String text) {
    try {
      if (text.toLowerCase().startsWith('0x')) {
        text = text.substring(2);
      }
      var keyInt = BigInt.parse(text, radix: 16);
      if (keyInt > BigInt.from(0) &&
          keyInt < ((BigInt.from(1) << 256) - BigInt.from(1))) {
        return keyInt;
      } else {
        throw Exception("invalid range");
      }
    } catch (err) {
      throw Exception("invalid key");
    }
  }

  static String uuid() {
    return Uuid(options: {'grng': UuidUtil.cryptoRNG}).v4();
  }
}

class EthereumKeyPair {
  // The EC private key
  final BigInt private;

  // The EC public key
  final String public;

  // TODO: Deprecated, migrate this to EthereumAddress
  // The ethereum address for this keypair
  final String addressString;

  EthereumAddress get address {
    return EthereumAddress.from(addressString);
  }

  const EthereumKeyPair({this.private, this.public, this.addressString});
}

class DartSecureRandom implements SecureRandom {
  Random random;

  DartSecureRandom() {
    // Dart's cryptographic random number source
    this.random = Random.secure();
  }

  @override
  int nextUint8() => random.nextInt(1 << 8);

  @override
  int nextUint16() => random.nextInt(1 << 16);

  @override
  int nextUint32() => random.nextInt(1 << 32);

  @override
  String get algorithmName => 'dart';

  // Inspired by code from Simon Binder (simolus3/web3dart)
  @override
  BigInt nextBigInteger(int bitLength) {
    final byteLength = bitLength ~/ 8;
    final remainderBits = bitLength % 8;
    final part1 = decode.decodeBigInt(nextBytes(byteLength));
    final part2 = BigInt.from(random.nextInt(1 << remainderBits));
    return part1 + (part2 << (byteLength * 8));
  }

  @override
  Uint8List nextBytes(int count) {
    var bytes = List<int>.generate(count, (_) => nextUint8());
    return Uint8List.fromList(bytes);
  }

  @override
  void seed(CipherParameters params) {
    throw Exception("Dart secure random is already seeded.");
  }
}

class StoredEthereumKey {
  // A locally unique id for referencing this key in the persistence layer.
  final String uid;

  // The time at which this key was stored.
  final DateTime time;

  // True if this key was imported.  False if it was generated by us.
  final bool imported;

  // The EC private key
  final BigInt private;

  StoredEthereumKey(
      {String uid,
      @required this.time,
      @required this.imported,
      @required this.private})
      : this.uid = uid ?? time.millisecondsSinceEpoch.toString();

  EthereumKeyPair _keyPair;

  EthereumAddress get address {
    return get().address;
  }

  EthereumKeyPair get() {
    if (_keyPair == null) {
      _keyPair = Crypto.fromPrivateKey(private);
    }
    return _keyPair;
  }

  StoredEthereumKeyRef ref() {
    return StoredEthereumKeyRef.from(uid);
  }

  StoredEthereumKey.fromJson(Map<String, dynamic> json)
      : this.uid = json['uuid'] ?? json['time'].toString(),
        this.time = DateTime.fromMillisecondsSinceEpoch(json['time']),
        this.imported = json['imported'],
        this.private = BigInt.parse(json['private']);

  Map<String, dynamic> toJson() => {
        'uuid': uid,
        'time': time.millisecondsSinceEpoch,
        'imported': imported,
        'private': private.toString()
      };

  /// Format the secret as a 64 character hex string, zero padded, without prefix.
  String formatSecretFixed() {
    return private.toRadixString(16).padLeft(64, '0');
  }

  static Map<String, StoredEthereumKey> map(List<StoredEthereumKey> list) {
    return {for (var key in list) key.uid: key};
  }

  static StoredEthereumKey find(List<StoredEthereumKey> list, String uid) {
    return list.firstWhere((key) => key.uid == uid, orElse: () => null);
  }

  static StoredEthereumKey generate() {
    var secret = Crypto.generateKeyPair().private;
    return StoredEthereumKey(
      imported: false,
      time: DateTime.now(),
      uid: Crypto.uuid(),
      private: secret,
    );
  }

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is StoredEthereumKey &&
          runtimeType == other.runtimeType &&
          uid == other.uid;

  @override
  int get hashCode => uid.hashCode;

  @override
  String toString() {
    return 'StoredEthereumKey{uid: $uid, address: $address, time: $time, imported: $imported}';
  }
}

// A simple referential value by uid.
// Note: We can do more with caching here if needed.
class StoredEthereumKeyRef {
  String keyUid;

  StoredEthereumKeyRef.from(String keyUid) {
    if (keyUid == null) {
      throw Exception("null key");
    }
    this.keyUid = keyUid;
  }

  // Resolve the reference
  Future<StoredEthereumKey> get() async {
    var keys = await UserPreferences().getKeys();
    try {
      return getFrom(keys);
    } catch (err) {
      throw Exception("get key error: $err");
    }
  }

  StoredEthereumKey getFrom(List<StoredEthereumKey> keys) {
    try {
      return keys.firstWhere((key) {
        return key.uid == keyUid;
      });
    } catch (err) {
      throw Exception("getFrom(key) error: $err");
    }
  }

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is StoredEthereumKeyRef &&
          runtimeType == other.runtimeType &&
          keyUid == other.keyUid;

  @override
  int get hashCode => keyUid.hashCode;

  @override
  String toString() {
    return keyUid;
  }
}

class EthereumAddress {
  // Used as null value in contract calls.
  static EthereumAddress zero =
      EthereumAddress.from('0x0000000000000000000000000000000000000000');

  BigInt value;

  EthereumAddress(BigInt value) {
    // Allow the string parser to validate.
    this.value = parse(value.toRadixString(16).toLowerCase().padLeft(40, '0'));
  }

  EthereumAddress.from(String text) {
    this.value = parse(text);
  }

  static fromNullable(String text) {
    return text == null ? null : EthereumAddress.from(text);
  }

  // TODO: EIP55
  // Display the optionally prefixed 40 char hex address.
  @override
  String toString({bool prefix: true}) {
    if (value == null) {
      throw Exception("invalid bigint");
    }
    var raw = value.toRadixString(16).padLeft(40, '0');
    var eip55 = checksumEthereumAddress(raw);
    return (prefix ? eip55 : Hex.remove0x(eip55));
  }

  // TODO: EIP55
  static BigInt parse(String text) {
    if (text == null) {
      throw Exception("invalid, null");
    }
    text = Hex.remove0x(text);
    if (text.length != 40) {
      throw Exception("invalid, length: $text");
    }
    try {
      var val = BigInt.parse(text, radix: 16);
      if (val < BigInt.from(0)) {
        throw Exception("invalid, range");
      }
      return val;
    } catch (err) {
      print(err);
      throw Exception("invalid, value");
    }
  }

  @override
  bool operator ==(other) {
    return other is EthereumAddress && value == other.value;
  }

  @override
  // TODO: implement hashCode
  int get hashCode => value.hashCode;
}

class Web3DartUtils {
  static MsgSignature web3Sign(Uint8List payload, StoredEthereumKey key) {
    final credentials = EthPrivateKey.fromHex(key.formatSecretFixed());
    //print("payload = ${hex.encode(payload)}");
    // Use web3 sign(), not web3 credentials.sign() which does a keccak256 on payload.
    return sign(payload, credentials.privateKey);
  }

  // Imitating what web3dart does
  static Uint8List padUint8ListTo32(Uint8List data) {
    return data.length == 32 ? data : Uint8List(32)
      ..setRange(32 - data.length, 32, data);
  }
}
