// ZorroTrader implementation of Stoken ACA tactical asset allocation strategy
//
// Read here  for more implementation details
// https://allocatesmartly.com/stokens-active-combined-asset-strategy/

// needs dividend adjusted price history for the following ETFs
#define ASSETS "SPY-US", "VNQ-US", "GLD-US", "IEF-US", "TLT-US"

// one algo per risk asset
#define ALGOS "SPY", "VNQ", "GLD"
const var NumAlgos = 3;

// should we reinvest profits?
const bool Reinvest = true;

// should we rebalance portfolio so to keep it 1/3 each slice
const bool DoRebalace = true;

// if true - then buy rist assets and hold them till the end
// useful as a benchmark
const bool BuyAndHold = false;

// set to true if one of slices allocation updated
bool isAllocationUpdated = false;

// initially selected [Asset] for allocation plot
string DefaultAsset;

var portfolioValue() {
  var Profit = ifelse(Reinvest, ProfitTotal, 0);
  return Capital + Profit;
}

// Calcualtes currently required number of lots for current asset
int lotsRequired() { return portfolioValue() / NumAlgos / priceC(); }

// Prints debug portfolio info to CSV file (not a real CSV)
void printPortfolio() {
  print(TO_CSV, "\n\nDate: %s", strdate(YMD, 0));
  string _algo, _asset;
  while (_algo = of(ALGOS)) {
    algo(_algo);
    while (_asset = of(ASSETS)) {
      asset(_asset);
      for (current_trades) {
        var value = TradeLots * priceC();
        print(TO_CSV, "\n%s:%s %.2f", Asset, Algo, value);
      }
    }
  }
  print(TO_CSV, "\nTotal: %.2f", portfolioValue());
}

// Plots allocation of current capital per algo
void plotAllocation() {
  var total = portfolioValue();

  int maybeNew = NEW;

  string _algo, _asset;
  while (_algo = of(ALGOS)) {
    algo(_algo);
    var algoTotal = 0;

    while (_asset = of(ASSETS)) {
      asset(_asset);
      for (current_trades) {
        algoTotal += TradeLots * priceC();
      }
    }

    int col;
    switch (Algo) {
    case "SPY":
      col = RED;
      break;
    case "VNQ":
      col = GREEN;
      break;
    case "GLD":
      col = BLUE;
      break;
    }

    // plot on the asset chart which was selected in the [Asset]
    //	combo box when test started
    asset(DefaultAsset);
    // print(TO_CSV, "\n %s: %2.f %2.f", "a", algoTotal, total);
    plot(Algo, algoTotal / total, maybeNew | SPLINE, col);
    if (maybeNew == NEW) {
      maybeNew = 0;
    }
  }
}

// Enters risk asset in the begginging
void initEnter(string riskAsset) {
  asset(riskAsset);
  enterLong(lotsRequired());
}

// Rebalance
void rebalance() {
  string _algo, _asset;
  while (_algo = of(ALGOS)) {
    algo(_algo);
    int algoLotsTotal = 0;
    string algoAsset = 0;

    while (_asset = of(ASSETS)) {
      // this should always be risk-on or risk-off asset for the given algo
      // but might have multiple trades due to previous rebalance
      asset(_asset);
      for (current_trades) {
        if (algoAsset == 0) {
          algoAsset = TradeAsset;
        } else if (algoAsset != TradeAsset) {
          printf("\nError: unexpected trade asset %s, expecting %s", TradeAsset,
                 algoAsset);
        }
        algoLotsTotal += TradeLots;
      }
    }

    if (algoAsset == 0) {
      printf("\nError: no active trades for algo %s", Algo);
      break;
    }
    asset(algoAsset);
    int lotsDelta = algoLotsTotal - lotsRequired();

    if (lotsDelta > 0) {
      // partial close
      exitLong(Algo, 0, lotsDelta);
    } else if (lotsDelta < 0) {
      // grow position
      enterLong(-lotsDelta);
    }
  }
}

// Runs Algo (1 algo per slice)
// each slice is defined by the risk/defensive assets pair
// and upper/lower channel period of the risk asset
void runAlgo(string riskAsset, string defensiveAsset, int riskOnPeriod,
             int riskOffPeriod) {
  asset(riskAsset);

  vars LagCloses = series(priceC(1));
  var UpperChannel = MaxVal(LagCloses, riskOnPeriod);
  var LowerChannel = MinVal(LagCloses, riskOffPeriod);

  // plot channels - useful for debugging
  plot("Upper", UpperChannel, 0, GREEN);
  plot("Lower", LowerChannel, 0, BLUE);

  if (priceC() > UpperChannel && NumOpenLong == 0) {
    // risk-on

    asset(defensiveAsset);
    exitLong();

    asset(riskAsset);
    enterLong(lotsRequired());
    isAllocationUpdated = true;
  } else if (priceC() < LowerChannel) {
    // risk-off

    exitLong();

    asset(defensiveAsset);
    if (NumOpenLong == 0) {
      enterLong(lotsRequired());
      isAllocationUpdated = true;
    }
  }
}

function run() {
  if (is(INITRUN)) {
    set(LOGFILE | PLOTNOW);
    StartDate = 20000101;
    EndDate = 20220905;
    LookBack = 252;
    Capital = 100000;
    BarPeriod = 1440;

    DefaultAsset = Asset;
    string _asset;
    while (_asset = of(ASSETS)) {
      assetAdd(_asset);
      asset(_asset);
    }
  }

  if (!is(LOOKBACK)) {

    // go risk-on assets
    static bool initBought = false;
    if (!initBought) {
      while (algo(loop(ALGOS))) {
        if (Algo == "SPY")
          initEnter("SPY-US");
        else if (Algo == "VNQ")
          initEnter("VNQ-US");
        else if (Algo == "GLD")
          initEnter("GLD-US");
      }
      initBought = true;
    }

    // check/switch risk-on/risk-off
    isAllocationUpdated = false;
    if (!BuyAndHold) {
      while (algo(loop(ALGOS))) {
        if (Algo == "SPY")
          runAlgo("SPY-US", "IEF-US", 126, 252);
        else if (Algo == "VNQ")
          runAlgo("VNQ-US", "IEF-US", 126, 252);
        else if (Algo == "GLD")
          runAlgo("GLD-US", "TLT-US", 252, 126);
      }
    }

    // rebalance if necessary
    static int lastYearRebalance = -1;
    if (DoRebalace && (isAllocationUpdated || year() != lastYearRebalance)) {
      rebalance();
      lastYearRebalance = year();
    }

    // plot and log debug information
    plotAllocation();
    // only print portfolio on the first day of month
    static int lastMonth = -1;
    if (month() != lastMonth) {
      lastMonth = month();
      printPortfolio();
    }
  }
}