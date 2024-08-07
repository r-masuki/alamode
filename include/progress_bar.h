//
// Created by Terumasa Tadano on 2024/05/10.
//

#pragma once

#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <cmath>

#ifdef _WIN32
#include <io.h>       // Windows header for _isatty() and file descriptor macros
#define isatty _isatty
#else
#include <unistd.h>   // POSIX header for isatty()
#endif

bool isOutputToConsole() {
    // File descriptor 1 is stdout
    bool isConsoleOutput = getenv("FORCE_CONSOLE_OUTPUT") != nullptr;
    return isConsoleOutput;
    //return isConsoleOutput || isatty(fileno(stdout));
}

void displayProgressBar(int current,
                        int total,
                        std::ostream& out,
                        long long timeRemaining,
                        bool isConsoleOutput,
                        const std::string prefix = "3ph",
                        const int width = 40) {

    float progress = (float)current / total;
    int pos = width * progress;

    // Calculate remaining time in minutes and seconds
    const auto hours = timeRemaining / 3600000;
    const auto minutes = (timeRemaining % 3600000) / 60000;
    const auto seconds = (timeRemaining % 60000) / 1000;

    // Start the progress bar with color
    if (isConsoleOutput) {
        out << "\033[32m"; // Green color start
    }
    out << " " << prefix << " [";

    for (int i = 0; i < width; ++i) {
        if (i < pos) out << "█";
        else out << " ";
    }

    out << "] ";
    out << int(progress * 100.0) << "%";
    out << " - ETA: "  << std::setw(3) << std::setfill('0') << hours
    << ":" << std::setw(2) << std::setfill('0') << minutes
    << ":" << std::setw(2) << std::setfill('0') << seconds;

    if (isConsoleOutput) {
        out << "\033[0m"; // Reset color
        out << "\r";      // Carriage return to overwrite the line in console
    } else {
        out << "\n";     // New line for file output
    }
    out << std::setfill(' '); // Reset fill character
    out.flush();
}

