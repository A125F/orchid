/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2019  The Orchid Authors
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */



#include <map>
#include <vector>
#include <math.h>
#include <functional>
#include <limits>


double Minutes = 60;
double Hours   = 60*Minutes;
double Days    = 24*Hours;
double Weeks   = 7*Days;

using namespace std;

double& CurrentTime()
{
	static double ctime = 0;
	return ctime;
}

template <class T>
map<string, T>& get_store()
{
	static std::map<std::string, T> store_;
	return store_;
}

template <class T> T persist_load(const char* k, T x)
{
	std::string key(k);
	auto& store = get_store<T>();
	if (store.find(key) == store.end()) {
		store[key] = x;
	}
	return store[key];
}

template <class T> void persist_store(const char* k, T x)
{
	std::string key(k);
	auto& store = get_store<T>();
	store[key] = x;
}


typedef unsigned int bytes32;
typedef unsigned int uint256;


double& get_OXT_balance(bytes32 account)  	// (RPC call, cached) external eth balance
{
	static map<bytes32, double> balances_;
	return balances_[account];
}


double& get_trans_cost() // (oracle) get current transaction cost estimate
{
	static double local_(0.1);
	return local_;
}

double& get_max_trans_cost()   // (oracle) get estimate of max reasonable current transaction fees
{
	static double local_(0.1);
	return local_;
}


struct ITickable
{
    virtual void on_timer() = 0;
};

vector<pair<ITickable*, double>>& GetTickables()
{
	static vector<pair<ITickable*, double>> store_; return store_;
}

void register_timer_func(ITickable* p, double t)
{
	GetTickables().push_back(pair<ITickable*,double>(p, t));
}


struct Lotpot { double amount_, escrow_; };

namespace RPC
{
	Lotpot& balance(bytes32 k) {
		map<bytes32, Lotpot> local_;
		return local_[k];
	}

	void grab(bytes32 secret, bytes32 hash, bytes32 target, bytes32 nonce, double ratio, double start, double range, double amount, bytes32 sig, vector<bytes32>)
	{

	}

}




struct IBudget
{
	virtual double get_afford() = 0;
	virtual double get_max_faceval() = 0;
    virtual void on_connect() = 0;
    virtual void on_disconnect() = 0;
    virtual void on_invoice() = 0;
};

struct Budget_PredExpW : public IBudget, public ITickable
{
	// budgeting params (input from UI)
	double budget_edate_;   	     // target budget end date
	double max_faceval_;            // maximum allowable faceval in OXT (user's hard variance limit)
	double max_prepay_time_ = 10;   // max amount of time client will prepay (credit) to server for undelivered bandwidth, in seconds (client->server trust)

    // config hyperparams
	double timer_sample_rate_ = 60; // frequency of calls to on_timer() (nothing special about 60s)

    // internal
	double prepay_credit_;
	double route_active_;                  // true when a connection/circuit is active
	double exp_half_life   = 1*Weeks;      // 1 one week 50% smoothing period
	double last_pay_date_;
	double spendrate_ = 0;
	bytes32 account_;


	virtual double get_afford() {
		double time_owed = (CurrentTime() - last_pay_date_);
		double allowed   = spendrate_ * (time_owed + prepay_credit_);
        return allowed;
    }
    
	virtual double get_max_faceval() {
        return max_faceval_;
    }

    void on_timer() {
    
        double ltime    = persist_load("ltime", CurrentTime()); // load variable from persistent disk/DB/config, default to CurrentTime()
        double wactive  = persist_load("wactive", 1.0);       

        // predict the fraction of time Orchid is active & connected (simple exp smoothing predictor) 
        double etime    = CurrentTime() - ltime;
        double decay    = exp(log(0.5) * etime/exp_half_life);        
        double cactive  = route_active_ ? 1.0 : 0.0;       
        wactive         = decay*wactive + (1-decay)*cactive;
        ltime           = CurrentTime();
        
        double pred_future_active_time = (budget_edate_ - CurrentTime()) * wactive;
        spendrate_      = get_OXT_balance(account_) / (pred_future_active_time + 1*Days);

        persist_store("ltime",   ltime);
        persist_store("wactive", wactive);
    }

    Budget_PredExpW(bytes32 account): account_(account) {
        register_timer_func(this, timer_sample_rate_); // call on_timer() every {timer_sample_rate_} seconds
        route_active_ = false;
        on_timer(); // tick the timer once on startup to update for any time orchid was shutdown
    }
    
    void on_connect()    { route_active_ = true;  last_pay_date_ = CurrentTime(); prepay_credit_ = max_prepay_time_; }
    void on_disconnect() { route_active_ = false; }
    void on_invoice()    { last_pay_date_ = CurrentTime(); prepay_credit_ = 0; }
};


struct Server;

template <class T>
string encodePacked(T x) { return to_string(x); }

template <class T, class ... Ts>
string encodePacked(T x, Ts... xs)
{
	return to_string(x) + encodePacked(xs...);
}

bytes32 keccak256(const string& x) 	{ return hash<string>{}(x); }
bytes32 sign(bytes32 x) 			{ return hash<bytes32>{}(x);}


struct payment
{
	bytes32 hash_secret, target, nonce, sender;
	double  ratio, start, range, amount;
	bytes32 sig;
};


double is_winner(payment p)  // offline check to see if ticket is a winner
{
	double dnonce = double(p.nonce) / double(numeric_limits<bytes32>::max());
	return dnonce < p.ratio;
}


struct Client : public ITickable
{
	// budgeting params (input from UI)
	double target_overhead_ = 0.1;	// target transaction fee overhead  // move to server?

	// Internal
	bytes32 account_;
	double last_pay_date_ = CurrentTime();
	IBudget* budget_;
	Server* server_;

	uint256 hash_secret_;
	bytes32 target_;

	Client() {
		account_ = rand();
		budget_  = new Budget_PredExpW(account_);
	}

	~Client() { delete budget_; }

    
	// External functions
	double get_trust_bound();  	// max OXT amount client is willing to lend to server (can initially be a large constant)

	// user initiates connection to server (random selection picks server)
	void on_connect(Server* server);
    
    // callback when disconnected from route
	void on_disconnect()     { budget_->on_disconnect(); }

	void on_invoice(uint256 hash_secret, bytes32 target, double bal_owed, double trans_cost) { hash_secret_ = hash_secret; target_ = target;}
	void on_sendpay(uint256 hash_secret, bytes32 target);

    void on_timer() { on_sendpay(hash_secret_, target_); }

};



struct Server
{
	// Server Hyperparams - from a config file or something
	double bytes_per_upay_;  // desired inv frequency of payments, in bytes
	double data_price_;	  // server's OXT/byte price
	double targ_balance_;    // target min client balance before invoice sent
	double min_balance_;     // min client balance to route packets (default 0)

	// Internal
	bytes32 account_;
	map<Client*, double> 	balances_;
	map<double, bytes32> 	old_tickets_;
	map<bytes32, bytes32> 	secrets_;
    
	// External functions
	//double is_winner(payment p);  // offline check to see if ticket is a winner
    	
	// Protocol client message handlers
	//void on_connect(Client* client);		    // client connection request
	//void on_upay(Client* client, payment* p);	// upayment from client
    	
	// On packet data event handlers
	//void on_out_packets(Client* client, string data);	// inc data from client out to target
	//void on_in_packets(Client* client, string data);   // inc data from target out to client

	void invoice_check(Client* client) {
		if (balances_[client] <= targ_balance_) {
		    double billed_amt  = max(bytes_per_upay_ * data_price_, targ_balance_ - balances_[client]);
		    auto secret = rand();
		    auto hash_secret = hash<bytes32>{}(secret);
		    secrets_[hash_secret] = secret;
		    //send("invoice", client, {hash_secret, target = client, billed_amt, trans_cost = get_trans_cost() });
		    client->on_invoice(hash_secret, account_, billed_amt, get_trans_cost());
		}
	}

	// client connection request
	void on_connect(Client* client) {
		invoice_check(client);
		// connection established, stuff happens		
	}

	// todo: remove sync ethnode RPC calls with async
	bool check_payment(payment p) { // validate the micropayment (does not verify it's a winner)
		if (RPC::balance(p.sender).amount_ <     p.amount) return false;
		if (RPC::balance(p.sender).escrow_ < 2 * p.amount) return false; // 2 is justin's magic number, and may change
	}

	void grab(payment p, double trans_fee) {
		//function grab(uint256 secret, bytes32 hash, address payable target, uint256 nonce, uint256 ratio, uint256 start, uint128 range, uint128 amount, uint8 v, bytes32 r, bytes32 s, bytes32[] memory old)
		auto hash = p.hash_secret;
		auto secret = secrets_[hash];
		bytes32 ticket = keccak256(encodePacked(hash, p.target, p.nonce, p.ratio, p.start, p.range, p.amount));
		//old_tickets_.insert(ticket, p.start + p.range);
		vector<bytes32> old = {};
		/* while ((CurrentTime() + 1*Hour) > old_tickets.top().key()) {
			old.insert(old_tickets.pop());
		} */
		//RPC::transaction(trans_fee, grab(secret, hash, p.target, p.nonce, p.ratio, p.start, p.range, p.amount, p.sig, old));
		RPC::grab(secret, hash, p.target, p.nonce, p.ratio, p.start, p.range, p.amount, p.sig, old);
	}

	double ticket_value(payment p, double trans_fee) {
		auto val = (p.amount - trans_fee) * p.ratio;
		auto timestamp = CurrentTime() + 5*Minutes; // 5 minutes is roughly upper eth transaction delay
		double limit = val;
		if (p.start >= timestamp)
			limit = val;
		else
			limit = (val * (p.range - (timestamp - p.start)) / p.range);
		return limit;
	}
	

	// upayment from client
	void on_upay(Client* client, payment p) { //  uses blocking external ethnode RPC calls
		if (check_payment(p)) { // validate the micropayment
			auto trans_fee = get_trans_cost();
			balances_[client] += ticket_value(p, trans_fee);
			if (is_winner(p)) { // now check to see if it's a winner, and redeem if so
				grab(p, trans_fee);
			}
		}
	}

	
	void bill_packet(Client* client, string data) {
		double cost = data.size() * data_price_;
		balances_[client] -= cost;
		invoice_check(client);
	}

	void on_out_packets(Client* client, string data) {	// inc data from client out to target
		bill_packet(client, data);
		if (balances_[client] > min_balance_); // { send(client.target, data); }
	}

	void on_in_packets(Client* client, string data) {   // inc data from target out to client
		bill_packet(client, data);
		if (balances_[client] > min_balance_); //  { send(client, data); }
	}


};



// user initiates connection to server (random selection picks server)
void Client::on_connect(Server* server) {
	server_ = server;
	//send("connect", server);
	server->on_connect(this);
	budget_->on_connect();
}



void Client::on_sendpay(uint256 hash_secret, bytes32 target)
{
	double afford	   	= budget_->get_afford();
	double max_face_val	= budget_->get_max_faceval();

	double exp_val 		= afford;
   	double trans_cost   = get_trans_cost();
	double face_val     = trans_cost / target_overhead_;
	//face_val      = min(face_val, max_face_val);
	if (face_val > max_face_val) face_val = 0;  // hard constraint failure

	// todo: simulate server ticket value function?
	if (face_val > trans_cost) {
		double win_prob = exp_val / (face_val - trans_cost);
		win_prob    	= max(min(win_prob, 1.0), 0.0); // todo: server may want a lower bound on win_prob for double-spend reasons
		// todo: is one hour duration (range) reasonable?
		bytes32 nonce = rand();
		double ratio = win_prob, start = CurrentTime(), range = 1.0*Hours, amount = face_val;
		bytes32 ticket = keccak256(encodePacked(hash_secret, target, nonce, ratio, start, range, amount));
		auto sig = sign(ticket);
		//send("upay", server_, payment{hash_secret, target,  nonce, ratio, start, range, amount, sig} );
		server_->on_upay(this, payment{hash_secret, target,  nonce, account_, ratio, start, range, amount, sig});
		budget_->on_invoice();
		// any book-keeping here
	}
	else {
		// handle payment error, report to GUI, etc
	}
}


/*
void Client::on_invoice(uint256 hash_secret, bytes32 target, double bal_owed, double trans_cost) { // server requests payment from client

	double afford	   	= budget_->get_afford();
	double max_face_val	= budget_->get_max_faceval();

	// todo: add back client side balance of trade tracking for trust bound

	//trust_bound 	= get_trust_bound();
	//exp_val     	= min(afford, bal_owed, trust_bound); // moved into budgeting
	double exp_val 		= min(afford, bal_owed);
   	trans_cost  		= min(get_max_trans_cost(), trans_cost);
	double face_val     = trans_cost / target_overhead_;
	//face_val      = min(face_val, max_face_val);
	if (face_val > max_face_val) face_val = 0;  // hard constraint failure

	// todo: simulate server ticket value function?
	if (face_val > trans_cost) {
		double win_prob = exp_val / (face_val - trans_cost);
		win_prob    	= max(min(win_prob, 1.0), 0.0); // todo: server may want a lower bound on win_prob for double-spend reasons
		// todo: is one hour duration (range) reasonable?
		bytes32 nonce = rand();
		double ratio = win_prob, start = CurrentTime(), range = 1.0*Hours, amount = face_val;
		bytes32 ticket = keccak256(encodePacked(hash_secret, target, nonce, ratio, start, range, amount));
		auto sig = sign(ticket);
		//send("upay", server_, payment{hash_secret, target,  nonce, ratio, start, range, amount, sig} );
		server_->on_upay(this, payment{hash_secret, target,  nonce, account_, ratio, start, range, amount, sig});
		budget_->on_invoice();
		// any book-keeping here
	}
	else {
		// handle payment error
	}
}
*/




/*
 *

R < C

(Sa/S)*N*F*Tb < Sa*Ir

N*F*Tb < S*Ir

Tb < (S*Ir) / (N*F)

$10M / 1M*365*24


Tb < (R-Cb) / (N*F)

Tb < Pu / F

Tb < Pu * Dc



The happy asshole free equilibrium is one where the asshole strategy is unprofitable: the earnings of the strategy (registering stake but providing zero service) is less than the cost:
R < C

(Sa/S)*N*F*Tb < Sa*Ir

Sa: attacker stake fraction
Ir: interest rate
S: total stake
N: number of users
F: connection frequency
Tb: effective 'trust bound' - the amount attacker earns from a connection before they reroll

N*F*Tb < S*Ir

Tb < (S*Ir) / (N*F)

If we have 1 million users and $500M staked and 10% interest rates, S*Ir is $50M/yr.  IF the avg connection frequency is 1 hour, then:

Tb < $50M / 1M*365*24 = $0.0057

$0.0057 is quite large - on the order of a thousand micropayments (which is not a coincidence, with payments sent on the order of seconds and a few thousand seconds in an hour)

And actually, we can simplify the equation:


First we can replace (S*Ir) with (R-Cb), the profit over time:

Tb < (R-Cb) / (N*F)

Then we can substitute Pu = (R-Cb)/N, where Pu is the profit over time per user

Tb < Pu / F

And finally replace 1/F with D, the duration:

Tb < Pu * D

So in essence, as long as the typical connection session lasts much longer than the time it takes a user/client to reroll a bad connection, and the budgeting algorithm doesn't prepay too much, this attack should be unprofitable.


The earlier end simplified equation:
Tb < Pu * D
Tb: effective 'trust bound' - the amount attacker earns from a connection before they reroll
Pu: profit over time per user
D:  connection duration

Perhaps more clear formulated as:
Pu * Rt < Pu * D
Pu: profit over time per user
Rt: time until user rerolls a bad connection
D:  connection duration


*/



