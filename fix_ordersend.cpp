//+------------------------------------------------------------------+
//| EA GOLD HEDGE RECOVERY PRO - LUXURY DASHBOARD EDITION            |
//+------------------------------------------------------------------+
#property strict

input double Lot                = 0.01;
input int    Magic              = 888;
input int    FastEMA            = 20;    
input int    SlowEMA            = 50;    
input int    TakeProfitPoints   = 300;   
input int    HedgeDistance      = 1500;  
input double HedgeMultiplier    = 1.8;   
input int    MaxSpread          = 350;   
input int    MaxOrder           = 5;     
input double TargetProfitUSD    = 1.5;   
input double MinProfitToClear   = 0.3;   

// Variabel Global
datetime TimeLastOrder = 0;

//=================== FUNGSI UI MEWAH ===================
void CreateLabel(string name, string text, int x, int y, int size, color clr, int anchor=0) {
   ObjectCreate(0, name, OBJ_LABEL, 0, 0, 0);
   ObjectSetInteger(0, name, OBJPROP_XDISTANCE, x);
   ObjectSetInteger(0, name, OBJPROP_YDISTANCE, y);
   ObjectSetInteger(0, name, OBJPROP_FONTSIZE, size);
   ObjectSetString(0, name, OBJPROP_TEXT, text);
   ObjectSetInteger(0, name, OBJPROP_COLOR, clr);
   ObjectSetInteger(0, name, OBJPROP_CORNER, anchor);
   ObjectSetString(0, name, OBJPROP_FONT, "Trebuchet MS");
}

void UpdateDashboard() {
   double prof = TotalGlobalProfit();
   color pColor = (prof >= 0) ? clrLime : clrRed;

   if(ObjectFind(0,"bg_box")<0) {
      ObjectCreate(0, "bg_box", OBJ_RECTANGLE_LABEL, 0, 0, 0);
      ObjectSetInteger(0, "bg_box", OBJPROP_XDISTANCE, 10);
      ObjectSetInteger(0, "bg_box", OBJPROP_YDISTANCE, 20);
      ObjectSetInteger(0, "bg_box", OBJPROP_XSIZE, 240);
      ObjectSetInteger(0, "bg_box", OBJPROP_YSIZE, 160);
      ObjectSetInteger(0, "bg_box", OBJPROP_BGCOLOR, clrBlack);
      ObjectSetInteger(0, "bg_box", OBJPROP_BORDER_COLOR, clrGoldenrod);
      ObjectSetInteger(0, "bg_box", OBJPROP_WIDTH, 2);
   }

   CreateLabel("lb_title", "GOLD HEDGE PRO V2.1", 25, 30, 10, clrGoldenrod);
   CreateLabel("lb_equity", "Equity: $" + DoubleToString(AccountEquity(), 2), 25, 55, 9, clrWhite);
   CreateLabel("lb_orders", "Active Orders: " + (string)CountOrders() + " / " + (string)MaxOrder, 25, 75, 9, clrWhite);
   CreateLabel("lb_spread", "Spread: " + DoubleToString(MarketInfo(Symbol(),MODE_SPREAD), 0), 25, 95, 9, clrWhite);
   CreateLabel("lb_profit", "Profit: $" + DoubleToString(prof, 2), 25, 120, 11, pColor);
   CreateLabel("lb_status", "Clearing Mode: " + (MinProfitToClear > 0 ? "AUTO" : "OFF"), 25, 145, 8, clrGray);
}

//=================== FUNGSI LOGIKA ===================
int CountOrders() {
   int total=0;
   for(int i=0; i<OrdersTotal(); i++) {
      if(OrderSelect(i,SELECT_BY_POS,MODE_TRADES)) {
         if(OrderMagicNumber()==Magic && OrderSymbol()==Symbol()) total++;
      }
   }
   return total;
}

double TotalGlobalProfit() {
   double total = 0;
   for(int i=0; i<OrdersTotal(); i++) {
      if(OrderSelect(i,SELECT_BY_POS,MODE_TRADES)) {
         if(OrderMagicNumber()==Magic && OrderSymbol()==Symbol()) {
            total += OrderProfit() + OrderSwap() + OrderCommission();
         }
      }
   }
   return total;
}

void CloseAll() {
   for(int i=OrdersTotal()-1; i>=0; i--) {
      if(OrderSelect(i,SELECT_BY_POS,MODE_TRADES)) {
         if(OrderMagicNumber()==Magic && OrderSymbol()==Symbol()) {
            RefreshRates();
            bool res = false;
            if(OrderType()==OP_BUY) res = OrderClose(OrderTicket(),OrderLots(),Bid,10,clrWhite);
            else if(OrderType()==OP_SELL) res = OrderClose(OrderTicket(),OrderLots(),Ask,10,clrWhite);
            if(!res) Print("Gagal Close Order: ", GetLastError());
         }
      }
   }
}

void OpenOrder(int type, double lots, string comment) {
   double pointValue = (Digits==3 || Digits==5) ? Point*10 : Point;
   double price = (type==OP_BUY) ? Ask : Bid;
   double tp = (type==OP_BUY) ? price + (TakeProfitPoints * pointValue) : price - (TakeProfitPoints * pointValue);

   int ticket = OrderSend(Symbol(), type, lots, price, 10, 0, tp, comment, Magic, 0, (type==OP_BUY?clrBlue:clrRed));
   if(ticket > 0) TimeLastOrder = Time[0]; 
}

void MultiHedgeLogic() {
   int currentOrders = CountOrders();
   if(currentOrders <= 0 || currentOrders >= MaxOrder || TimeLastOrder == Time[0]) return; 

   double lastLot = 0; int lastType = -1; double lastPrice = 0; datetime lastTime = 0;
   for(int i=0; i<OrdersTotal(); i++) {
      if(OrderSelect(i,SELECT_BY_POS,MODE_TRADES)) {
         if(OrderMagicNumber()==Magic && OrderSymbol()==Symbol()) {
            if(OrderOpenTime() > lastTime) {
               lastTime = OrderOpenTime(); lastLot = OrderLots(); lastType = OrderType(); lastPrice = OrderOpenPrice();
            }
         }
      }
   }

   double pointValue = (Digits==3 || Digits==5) ? Point*10 : Point;
   if(MathAbs(lastPrice - (lastType==OP_BUY ? Bid : Ask)) >= HedgeDistance * pointValue) {
      OpenOrder((lastType == OP_BUY ? OP_SELL : OP_BUY), NormalizeDouble(lastLot * HedgeMultiplier, 2), "HEDGE_" + (string)currentOrders);
   }
}

//=================== EKSEKUSI UTAMA ===================
void OnTick() {
   UpdateDashboard();
   if(MarketInfo(Symbol(),MODE_SPREAD) > MaxSpread) return;
   if(CountOrders() > 0 && TotalGlobalProfit() >= TargetProfitUSD) { CloseAll(); return; }
   
   if(CountOrders() > 1 && TotalGlobalProfit() >= MinProfitToClear) {
      double rugiTerbesar = 0; int ticketTeburuk = -1;
      for(int i=0; i<OrdersTotal(); i++) {
         if(OrderSelect(i,SELECT_BY_POS,MODE_TRADES) && OrderMagicNumber()==Magic) {
            double p = OrderProfit() + OrderSwap() + OrderCommission();
            if(p < rugiTerbesar) { rugiTerbesar = p; ticketTeburuk = OrderTicket(); }
         }
      }
      if(ticketTeburuk != -1 && TotalGlobalProfit() > (MathAbs(rugiTerbesar) + 0.1)) CloseAll();
   }

   if(CountOrders() == 0 && TimeLastOrder != Time[0]) {
      double f = iMA(NULL,0,FastEMA,0,MODE_EMA,PRICE_CLOSE,1);
      double s = iMA(NULL,0,SlowEMA,0,MODE_EMA,PRICE_CLOSE,1);
      if(f > s && iClose(NULL,0,1) > iOpen(NULL,0,1)) OpenOrder(OP_BUY, Lot, "MAIN");
      else if(f < s && iClose(NULL,0,1) < iOpen(NULL,0,1)) OpenOrder(OP_SELL, Lot, "MAIN");
   }
   MultiHedgeLogic();
}

void OnDeinit(const int reason) { ObjectsDeleteAll(0, "bg_"); ObjectsDeleteAll(0, "lb_"); }