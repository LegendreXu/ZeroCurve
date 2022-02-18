#include "CurveData.h"
#include <string>
#include <fstream>
#include <cstring>
#include <sstream>
#include <vector>

using std::string;
using std::ifstream;
using std::ofstream;
using std::stringstream;
using std::stoi;
using std::find;

#define CurveDatalog	"C:\\temp\\CurveData.log"

const char* MONTH[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

long
string2date(string data)
{
	size_t pos1 = data.find('-');
	size_t pos2 = data.rfind('-');
	int day = stoi(data.substr(0, pos1));
	int year = 2000 + stoi(data.substr(pos2 + 1));
	vector<string> m(MONTH, MONTH + 12);
	int month = 1 + find(m.begin(), m.end(), \
		data.substr(pos1 + 1, pos2 - pos1 - 1)) - m.begin();
	return year * 10000 + month * 100 + day;
}

bool
CurveData::load(const char* filename) {
	ifstream fin(filename);
	string line, item;
	cash = new CashInput();
	futures = new FuturesInput();
	swaps = new SwapsInput();
	while (getline(fin, line)) {
		vector<string> data, months;
		stringstream ss(line);
		while (getline(ss, item, ',')) data.push_back(item);
		if (data[0] == "Currency") {
			currency = new char[data[1].size() + 1];
			memcpy(currency, data[1].c_str(), data[1].size() + 1);
		}
		else if (data[0] == "Base Date") {
			baseDate = string2date(data[1]);
		}
		else if (data[0] == "Days to Spot") {
			daysToSpot = stoi(data[1]);
		}
		else if (data[0] == "Cash Basis") {
			cash->m_cashBasis = data[1];
		}
		else if (data[0] == "Futures Basis") {
			futures->m_futuresBasis = data[1];
		}
		else if (data[0] == "Swaps Basis") {
			swaps->m_swapsBasis = data[1];
		}
		else if (data[0] == "Swaps Freq") {
			swaps->m_swapsFreq = data[1];
		}
		else if (data[0] == "Holiday") {
			holidayCenter = new char[data[1].size() + 1];
			memcpy(holidayCenter, data[1].c_str(), data[1].size() + 1);
		}
		else if (data[0] == "Cash") {
			CashPoint p(data[1], atof(data[2].c_str()));
			cash->m_cashPoints.push_back(p);
		}
		else if (data[0] == "Futures") {
			FuturesPoint p(string2date(data[1]), atof(data[2].c_str()));
			futures->m_futuresPoints.push_back(p);
		}
		else if (data[0] == "Swaps") {
			SwapsPoint p(data[1], atof(data[2].c_str()));
			swaps->m_swapsPoints.push_back(p);
		}
	}
	return SUCCESS;
}
