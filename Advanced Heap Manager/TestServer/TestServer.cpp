#include "Benchmark.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <vector>
#include <ctime>
#include <cstdlib>

// Number of times each configuration is repeated (the median is used in the table)
static const int REPETITIONS = 3;

static double median(std::vector<double> v)
{
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

int main()
{
    srand((unsigned)time(nullptr));

    int clients, messages;
    std::cout << "Clients: "; std::cin >> clients;
    std::cout << "Messages per client: "; std::cin >> messages;

    std::vector<double> tMalloc, tMap, tHeader;

    for (int rep = 1; rep <= REPETITIONS; ++rep)
    {
        std::cout << "\n##### Repetition " << rep << "/" << REPETITIONS << " #####\n";

        tMalloc.push_back(RunBenchmark(false, clients, messages).elapsedSeconds);
        std::cout << "malloc/free : " << tMalloc.back() << " s\n";

        tMap.push_back(RunBenchmark(true, clients, messages, AHM::Strategy::Map).elapsedSeconds);
        std::cout << "AHM (Map)   : " << tMap.back() << " s\n";

        tHeader.push_back(RunBenchmark(true, clients, messages, AHM::Strategy::Header).elapsedSeconds);
        std::cout << "AHM (Header): " << tHeader.back() << " s\n";
    }

    // ---------- Results table (medians) ----------
    const double totalMsgs = double(clients) * double(messages);

    std::cout << "\n========================================\n";
    std::cout << "Results (median of " << REPETITIONS << " repetitions)\n";
    std::cout << "Clients: " << clients << ", messages per client: " << messages << "\n";
    std::cout << "========================================\n";
    std::cout << std::left << std::setw(16) << "Mode"
        << std::setw(11) << "Median(s)"
        << std::setw(11) << "Min(s)"
        << std::setw(11) << "Max(s)"
        << "Throughput(msg/s)\n";
    std::cout << std::string(65, '-') << "\n";

    auto row = [&](const char* name, const std::vector<double>& t)
        {
            const double m = median(t);
            std::cout << std::left << std::setw(16) << name
                << std::fixed << std::setprecision(3)
                << std::setw(11) << m
                << std::setw(11) << *std::min_element(t.begin(), t.end())
                << std::setw(11) << *std::max_element(t.begin(), t.end())
                << std::setprecision(0) << (totalMsgs / m) << "\n";
        };
    row("malloc/free", tMalloc);
    row("AHM (Map)", tMap);
    row("AHM (Header)", tHeader);

    const double mm = median(tMalloc), mp = median(tMap), mh = median(tHeader);
    std::cout << std::fixed << std::setprecision(2)
        << "\nMap    vs malloc : " << (mm / mp) << "x\n"
        << "Header vs malloc : " << (mm / mh) << "x\n"
        << "Header vs Map    : " << (mp / mh) << "x\n"
        << "(A value > 1.00 means the first method is faster than the second)\n";

    // ---------- CSV export (appends results, allowing multiple client-count runs) ----------
    const char* csvPath = "server_results.csv";
    const bool csvExists = std::ifstream(csvPath).good();
    std::ofstream csv(csvPath, std::ios::app);
    if (!csvExists)
        csv << "clients,messages,mode,median_s,min_s,max_s,throughput_msg_s\n";

    auto dump = [&](const char* name, const std::vector<double>& t)
        {
            const double m = median(t);
            csv << clients << "," << messages << "," << name << ","
                << std::fixed << std::setprecision(4) << m << ","
                << *std::min_element(t.begin(), t.end()) << ","
                << *std::max_element(t.begin(), t.end()) << ","
                << std::setprecision(1) << (totalMsgs / m) << "\n";
        };
    dump("malloc", tMalloc);
    dump("ahm_map", tMap);
    dump("ahm_header", tHeader);
    csv.close();
    std::cout << "\nResults appended to server_results.csv\n";

    std::cin.ignore(); std::cin.get();
    return 0;
}