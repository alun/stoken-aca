// ZorroTrader implementation of Stoken ACA tactical asset allocation strategy
// https://allocatesmartly.com/stokens-active-combined-asset-strategy/

// needs dividend adjusted price history for the following ETFs
//	SPY-US
//	VNQ-US
//	GLD-US
//	IEF-US
//	TLT-US
//

const var NumAlgos = 3;
const bool Reinvest = false;

// print debug portfolio info to CSV file (not a real CSV)
void printPortfolio() {
	string _asset, _algo;
	print(TO_CSV, "\n\nDate: %s",strdate(YMD,0));
	var total = 0;
	for(open_trades) { 
		asset(TradeAsset);
		algo(TradeAlgo);
		
		var value = TradeLots * priceC();
		if (value > 0) {				
			print(TO_CSV, "\n%s %s: %.2f", Algo, Asset, value);
			total += value;
		}		
	}
	print(TO_CSV, "\nTotal: %.2f", total);
}

void initEnter(string riskAsset) {
	asset(riskAsset);
	int lots = Capital / NumAlgos / priceC();		
	enterLong(lots);			
}

void runAlgo(
	string riskAsset,
	string defensiveAsset,
	int riskOnPeriod,
	int riskOffPeriod
) {
	asset(riskAsset);
	
	vars LagCloses = series(priceC(1));
	var UpperChannel = MaxVal(LagCloses, riskOnPeriod);
	var LowerChannel = MinVal(LagCloses, riskOffPeriod);
	
	// plot channels - useful for debugging
	plot("Upper", UpperChannel, 0, 0x0000FF00);
	plot("Lower", LowerChannel, 0, 0x000000FF);
	
	if (priceC() > UpperChannel && NumOpenLong == 0) {
		// risk-on
		
		asset(defensiveAsset);
		exitLong();
		
		asset(riskAsset);
		var Profit = ifelse(Reinvest, ProfitTotal, 0);
		int lots = (Capital + Profit) / NumAlgos / priceC();
		enterLong(lots);
	}
	else if (priceC() < LowerChannel) {
		// risk-off
		
		exitLong();
		
		asset(defensiveAsset);
		if (NumOpenLong == 0) {
			var Profit = ifelse(Reinvest, ProfitTotal, 0);
			int lots = (Capital + Profit) / NumAlgos / priceC();
			enterLong(lots);
		}
	}
}

function run() {
	if (is(INITRUN)) {
		set(LOGFILE|PLOTNOW);
		StartDate = 20030101;
		LookBack = 252;	
		Capital = 100000;
		BarPeriod = 1440;
		
		assetAdd("SPY-US");
		assetAdd("VNQ-US");	
		assetAdd("GLD-US");
		assetAdd("IEF-US");
		assetAdd("TLT-US");	
	}
	
	static bool initBought = false;
	
	// go risk-on assets initially
	if (!is(LOOKBACK) && !initBought) {
		while(algo(loop("SPY", "VNQ", "GLD"))) {
			if (Algo == "SPY") initEnter("SPY-US");
			else if (Algo == "VNQ") initEnter("VNQ-US");
			else if (Algo == "GLD") initEnter("GLD-US");
		}
		initBought = true;
	}
	
	while(algo(loop("SPY", "VNQ", "GLD"))) {
		if (Algo == "SPY") runAlgo("SPY-US", "IEF-US", 126, 252);
		else if (Algo == "VNQ") runAlgo("VNQ-US", "IEF-US", 126, 252);
		else if (Algo == "GLD") runAlgo("GLD-US", "TLT-US", 252, 126);
	}
	
	if (!is(LOOKBACK) && day() == 1) {
		printPortfolio();
	}
}