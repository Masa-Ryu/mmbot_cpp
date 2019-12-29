/*
 * mtrader.h
 *
 *  Created on: 15. 5. 2019
 *      Author: ondra
 */

#ifndef SRC_MAIN_MTRADER_H_
#define SRC_MAIN_MTRADER_H_
#include <deque>
#include <optional>
#include <type_traits>

#include <shared/ini_config.h>
#include <imtjson/namedEnum.h>
#include "idailyperfmod.h"
#include "istatsvc.h"
#include "storage.h"
#include "report.h"
#include "strategy.h"

class IStockApi;

enum class Dynmult_mode {
	disabled,
	independent,
	together,
	alternate,
	half_alternate,
};


extern json::NamedEnum<Dynmult_mode> strDynmult_mode;

struct MTrader_Config {
	std::string pairsymb;
	std::string broker;
	std::string title;

	double buy_mult;
	double sell_mult;
	double buy_step_mult;
	double sell_step_mult;
	double min_size;
	double max_size;
	std::optional<double> min_balance;
	std::optional<double> max_balance;

	double dynmult_raise;
	double dynmult_fall;
	Dynmult_mode dynmult_mode;

	unsigned int accept_loss;

	double force_spread;
	double report_position_offset;

	unsigned int spread_calc_stdev_hours;
	unsigned int spread_calc_sma_hours;

	bool dry_run;
	bool internal_balance;
	bool detect_manual_trades;
	bool enabled;
	bool dust_orders;
	bool dynmult_scale;

	Strategy strategy = Strategy(nullptr);

	void loadConfig(json::Value data, bool force_dry_run);

};


class MTrader {
public:

	using StoragePtr = PStorage;
	using Config = MTrader_Config;

	struct Order: public IStockApi::Order {
		bool isSimilarTo(const Order &other, double step, bool inverted);
		Order(const IStockApi::Order& o):IStockApi::Order(o) {}
		Order() {}
		Order(double size, double price){
			this->size = size;
			this->price = price;
		}
	};

	struct OrderPair {
		std::optional<Order> buy,sell;
		static OrderPair fromJSON(json::Value json);
		json::Value toJSON() const;
	};


	MTrader(IStockSelector &stock_selector,
			StoragePtr &&storage,
			PStatSvc &&statsvc,
			Config config);



	void perform(bool manually);

	void init();

	OrderPair getOrders();
	void setOrder(std::optional<Order> &orig, Order neworder);


	using ChartItem = IStatSvc::ChartItem;


	struct Status {
		IStockApi::Ticker ticker;
		double curPrice;
		double curStep;
		double assetBalance;
		double internalBalance;
		double currencyBalance;
		double new_fees;
		IStockApi::TradesSync new_trades;
		ChartItem chartItem;
	};

	Status getMarketStatus() const;

	Order calculateOrder(double lastTradePrice,
			double step,
			double dynmult,
			double curPrice,
			double balance,
			double currency,
			double mult) const;
	Order calculateOrderFeeLess(
			double lastTradePrice,
			double step,
			double dynmult,
			double curPrice,
			double balance,
			double currency,
			double mult) const;

	const Config &getConfig() {return cfg;}

	const IStockApi::MarketInfo getMarketInfo() const {return minfo;}

	bool eraseTrade(std::string_view id, bool trunc);
	void reset();
	void repair();
	ondra_shared::StringView<IStatSvc::ChartItem> getChart() const;
	void dropState();
	void stop();

	using TradeHistory = std::vector<IStatSvc::TradeRecord>;

	const TradeHistory &getTrades() const;

	static std::string_view vtradePrefix;

	Strategy getStrategy() const {return strategy;}
	IStockApi &getBroker() {return stock;}

	struct VisRes {
		struct Item {
			double price, low, high, size;
			std::uint64_t time;
		};
		std::vector<Item> chart;
	};
	VisRes visualizeSpread(double sma, double stdev, double mult, double dyn_raise, double dyn_fall, json::StrViewA dynMode);
	static std::optional<double> getInternalBalance(const MTrader *ptr);

protected:
	class DynMultControl {
	public:
		DynMultControl(double raise, double fall, Dynmult_mode mode):raise(raise),fall(fall),mode(mode),mult_buy(1),mult_sell(1) {}

		void setMult(double buy, double sell);
		double getBuyMult() const;
		double getSellMult() const;

		double raise_fall(double v, bool israise);
		void update(bool buy_trade,bool sell_trade);

	protected:

		double raise;
		double fall;
		Dynmult_mode mode;
		double mult_buy;
		double mult_sell;

	};

	std::unique_ptr<IStockApi> ownedStock;
	IStockApi &stock;
	Config cfg;
	IStockApi::MarketInfo minfo;
	StoragePtr storage;
	PStatSvc statsvc;
	Strategy strategy;
	DynMultControl dynmult;
	bool need_load = true;
	bool recalc = true;
	json::Value test_backup;
	json::Value lastTradeId = nullptr;

	using TradeItem = IStockApi::Trade;
	using TWBItem = IStatSvc::TradeRecord;

	std::vector<ChartItem> chart;
	TradeHistory trades;

	std::optional<double> internal_balance;
	std::optional<double> currency_balance_cache;
	size_t magic = 0;
	size_t uid = 0;
	PerformanceReport tempPr;

	void loadState();
	void saveState();

	double raise_fall(double v, bool raise) const;

	static IStockApi &selectStock(IStockSelector &stock_selector, const Config &conf, std::unique_ptr<IStockApi> &ownedStock);

	void processTrades(Status &st);

	void update_dynmult(bool buy_trade,bool sell_trade);

	void acceptLoss(const Status &st, double dir);
	json::Value getTradeLastId() const;

	double calcSpread() const;
	bool checkMinMaxBalance(double newBalance, double dir) const;
	double limitOrderMinMaxBalance(double balance, double orderSize) const;

};




#endif /* SRC_MAIN_MTRADER_H_ */
