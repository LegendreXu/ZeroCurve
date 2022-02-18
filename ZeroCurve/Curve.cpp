#include "Curve.h"
#include <cstring>

julianDate
long2date(long ymd) {
	int year = ymd / 10000;
	int month = (ymd % 10000) / 100;
	int day = ymd % 100;
	return date::YMD2julianDate(year, month, day);
}

Curve::Curve(char* ccy, long baseDt, int spot, MMCalendar* pMMCal_,
	CashInput* cin, FuturesInput* fin, SwapsInput* sin)
	: m_currency(ccy), m_daysToSpot(spot), m_pMMCal(pMMCal_),
	m_pCashInput(cin), m_pFuturesInput(fin), m_pSwapsInput(sin) {
	cashYB = 360, futuresYB = 360, swapsYB = 365, nPerYear = 2;
	m_baseDate = long2date(baseDt);
	initProcess();
	processCash();
	processFutures();
	processSwaps();
}

bool
Curve::insertKeyPoint(date dt, DiscountFactorType df) {
	m_keyPoints.insert(KeyPoint(dt, df));
	return SUCCESS;
}

KeyPoints::const_iterator
Curve::firstKeyPoint() {
	if (m_keyPoints.size() == 0)
		return m_keyPoints.end();
	else
		return m_keyPoints.begin();
}

KeyPoint
Curve::retrieveKeyPoint(KeyPoints::const_iterator ki) {
	return *ki;
}

void
Curve::initProcess() {
	m_keyPoints.insert(KeyPoint(m_baseDate, 1));
}

double
Curve::interpolate(date dt) {
	if (m_keyPoints.count(dt))
		return m_keyPoints[dt];
	else {
		KeyPoints::iterator prev;
		for (auto it = m_keyPoints.begin(); it != m_keyPoints.end(); prev = it, it++) {
			if (it->first > dt) {
				KeyPoint p2 = retrieveKeyPoint(it);
				KeyPoint p1 = retrieveKeyPoint(prev);
				return p1.second * pow((p2.second / p1.second), double((dt - p1.first)) / (p2.first - p1.first));
			}
		}
		return 0.0;
	}
}

void
Curve::processCash() {
	DiscountFactorType df_on, df_spot, df_3m;
	double cash_on, cash_tn, cash_3m;
	date date_on, date_spot, date_3m;
	vector<CashPoint> cashPoints = m_pCashInput->m_cashPoints;
	cash_on = cashPoints[0].second;
	cash_tn = cashPoints[1].second;
	cash_3m = cashPoints[2].second;
	date_on = date(m_baseDate);
	m_pMMCal->addBusDays(date_on, 1);
	date_spot = date(m_baseDate);
	m_pMMCal->addBusDays(date_spot, 2);
	date_3m = date(m_baseDate);
	m_pMMCal->addBusDays(date_3m, 2); // to spot
	m_pMMCal->addMonths(date_3m, 3);  // spot to 3m
	df_on = 1 / (1 + cash_on * (date_on - m_baseDate) / cashYB);
	df_spot = df_on / (1 + cash_tn * (date_spot - date_on) / cashYB);
	df_3m = df_spot / (1 + cash_3m * (date_3m - date_spot) / cashYB);
	insertKeyPoint(date_on, df_on);
	insertKeyPoint(date_spot, df_spot);
	insertKeyPoint(date_3m, df_3m);
	dt3M = date_3m;
}

void
Curve::processFutures() {
	DiscountFactorType df_1st, df_2nd, df_3m, last_df;
	date date_1st, date_2nd, last_date;
	double base, exponent, price_1st;
	// stage one : df_1st, df_2nd
	df_3m = m_keyPoints[dt3M];
	vector<FuturesPoint> futurePoints = m_pFuturesInput->m_futuresPoints;
	date_1st = long2date(futurePoints[0].first);
	date_2nd = long2date(futurePoints[1].first);
	price_1st = futurePoints[0].second;
	base = 1 / (1 + ((100 - price_1st) / 100 * (date_2nd - date_1st) / futuresYB));
	exponent = double((dt3M - date_1st)) / (date_2nd - date_1st);
	df_1st = df_3m / pow(base, exponent);
	df_2nd = df_1st * base;
	insertKeyPoint(date_1st, df_1st);
	insertKeyPoint(date_2nd, df_2nd);
	// stage two : the rest of future df
	last_date = date_2nd;
	last_df = df_2nd;
	for (int i = 1; i < futurePoints.size(); i++) {
		double price = futurePoints[i].second;
		date dt = long2date(futurePoints[i].first);
		dt = m_pMMCal->nextIMMDay(dt);
		DiscountFactorType df;
		df = last_df / (1 + (100 - price) / 100 * (dt - last_date) / futuresYB);
		insertKeyPoint(dt, df);
		last_date = dt;
		last_df = df;
	}
}

double
searchK(SwapsCashFlows swapsFlows, vector<int> cashCount, vector<int> dfCount,
	int swapsYB, DiscountFactorType last_df, RateType rate) {
	// lhs = 100D0 - c1D1 - c2D2 -c3D3 - ... cnDn
	// rhs = cn+1Dn+1 + ... cn+kDn+k + 100Dn+k
	double lhs = 0, rhs = 0, left_k = 0, right_k = 1, mid_k = 0;
	for (auto flow : swapsFlows) 
		lhs += flow.CF * flow.DF;
	while (abs(lhs - rhs) > 1e-9) {
		rhs = 0;
		mid_k = 0.5 * (right_k + left_k);
		for (int i = 0; i < cashCount.size(); i++) {
			double cash, df;
			if (i == cashCount.size() - 1)
				cash = 100 * (1 + rate * cashCount[i] / swapsYB);
			else
				cash = 100 * rate * cashCount[i] / swapsYB;
			df = last_df * pow(mid_k, dfCount[i]);
			rhs = rhs + cash * df;
		}
		if (lhs > rhs) 
			left_k = mid_k;
		else
			right_k = mid_k;
	}
	return mid_k;
}

void
Curve::processSwaps() {
	// put D_0
	SwapsCashFlows swapsFlows;
	date date_spot = date(m_baseDate);
	m_pMMCal->addBusDays(date_spot, 2);
	swapsFlows.push_back(SwapsCashFlow(date_spot, 100, m_keyPoints[date_spot]));
	//start swaps flow
	int maturity, last_maturity = 2;
	for (auto point : m_pSwapsInput->m_swapsPoints) {
		maturity = stoi(point.first.substr(0, point.first.find('Y')));
		double rate = point.second;
		// put in known year swaps flow D1...D2n
		for (int i = 1; i <= last_maturity * nPerYear; i++) {
			date dt = date(date_spot);
			m_pMMCal->addMonths(dt, i * 6);
			date last_date = swapsFlows.back().dt;
			double cf = -100 * rate * (dt - last_date) / swapsYB;
			double df = interpolate(dt);
			swapsFlows.push_back(SwapsCashFlow(dt, cf, df));
			insertKeyPoint(dt, df);
		}
		// search K, put results in swaps flow and update m_keyPoints
		vector<int> cashCount; // for cash flow
		vector<int> dfCount; // for pow to calculate df
		vector<date> dates;
		date last_date = m_keyPoints.rbegin()->first;
		DiscountFactorType last_df = m_keyPoints.rbegin()->second;
		for (int i = last_maturity * nPerYear + 1; i <= maturity * nPerYear; i++) {
			date dt = date(date_spot);
			m_pMMCal->addMonths(dt, i * 6);
			dfCount.push_back(dt - m_keyPoints.rbegin()->first);
			if (dates.empty())
				cashCount.push_back(dt - swapsFlows.back().dt);
			else
				cashCount.push_back(dt - dates.back());
			dates.push_back(dt);
		}
		double k = searchK(swapsFlows, cashCount, dfCount, swapsYB, last_df, rate);
		for (int i = 0; i < (maturity - last_maturity) * nPerYear; i++) {
			insertKeyPoint(dates[i], last_df * pow(k, dfCount[i]));
		}
		// clear all the cashflow except D_0 to be ready for next time calculation,
		// and update some parameters
		swapsFlows.resize(1);
		last_maturity = maturity;
	}
}

SwapsCashFlow::SwapsCashFlow(date d, double cf, double df) : dt(d), CF(cf), DF(df) {

}
