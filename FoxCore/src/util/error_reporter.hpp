#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstdio>

// ���� ANSI terminal support ����������������������������������������������������������������������
#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

inline bool _ansiSupported() {
#ifdef _WIN32
    static bool checked = false;
    static bool supported = false;
    if (!checked) {
        checked = true;
        HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(hOut, &mode)) {
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                if (SetConsoleMode(hOut, mode)) {
                    supported = true;
                }
            }
        }
    }
    return supported;
#else
    return true;
#endif
}

#define RST  (_ansiSupported() ? "\x1b[0m" : "")
#define BLD  (_ansiSupported() ? "\x1b[1m" : "")
#define DIM  (_ansiSupported() ? "\x1b[2m" : "")
#define RED  (_ansiSupported() ? "\x1b[31m" : "")
#define YLW  (_ansiSupported() ? "\x1b[33m" : "")
#define CYN  (_ansiSupported() ? "\x1b[36m" : "")
#define BLU  (_ansiSupported() ? "\x1b[34m" : "")

// ���� singleton state (shared across TUs via Meyer's singleton) ��
class _ErrState {
    std::string  m_filename;
    std::vector<std::string> m_lines;
    bool         m_has_error = false;
    _ErrState() = default;
public:
    static _ErrState& get() { static _ErrState s; return s; }

    void setSource(const std::string& file, const std::string& code) {
        m_filename = file;
        m_lines.clear();
        std::istringstream iss(code);
        std::string ln;
        while (std::getline(iss, ln)) m_lines.push_back(ln);
    }

    const std::string& filename() const { return m_filename; }
    const std::string& getLine(int n) const {
        static const std::string empty;
        if (n > 0 && n <= (int)m_lines.size()) return m_lines[n - 1];
        return empty;
    }
    bool& hasError() { return m_has_error; }
};

// ���� helper ������������������������������������������������������������������������������������������������������
inline void _printSrcLine(int ln, int col_start, int col_end, const std::string& msg) {
    std::string l = _ErrState::get().getLine(ln);
    if (l.empty()) return;
    for (auto& c : l) if (c == '\t') c = ' ';

    std::cerr << DIM << "   |" << RST << std::endl;
    std::cerr << BLU << " " << ln << BLU << " | " << RST << l << std::endl;
    std::cerr << DIM << "   | " << RST;
    if (col_start > 0) std::cerr << std::string(col_start - 1, ' ');
    int len = col_end > col_start ? col_end - col_start : 1;
    std::cerr << RED << BLD;
    for (int i = 0; i < len && i < 60; ++i) std::cerr << "^";
    std::cerr << RST;
    if (!msg.empty()) std::cerr << " " << msg;
    std::cerr << std::endl;
    std::cerr << DIM << "   |" << RST << std::endl;
}

inline void _printLoc(const std::string& filename, int line, int col) {
    if (!filename.empty() && line > 0) {
        std::cerr << "  " << CYN << "-->" << RST
                  << " " << filename << ":" << line << ":" << col << std::endl;
    }
}

// ���� parse "L:C:" prefix from a message string ��������������������������������
inline bool _parseLineCol(const std::string& s, int& line, int& col, std::string& rest) {
    line = col = 0;
    rest = s;
    size_t c1 = s.find(':');
    if (c1 == std::string::npos) return false;
    size_t c2 = s.find(':', c1 + 1);
    if (c2 == std::string::npos) return false;

    std::string lp = s.substr(0, c1);
    std::string cp = s.substr(c1 + 1, c2 - c1 - 1);
    auto allDigits = [](const std::string& x) {
        if (x.empty()) return false;
        for (char ch : x) if (ch < '0' || ch > '9') return false;
        return true;
    };
    if (!allDigits(lp) || !allDigits(cp)) return false;

    line = std::stoi(lp);
    col  = std::stoi(cp);
    rest = s.substr(c2 + 1);
    if (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);
    return true;
}

// �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
//  Public API  (callable via ErrorReporter::func(...))
// �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

namespace ErrorReporter {

inline void setSource(const std::string& file, const std::string& code) {
    _ErrState::get().setSource(file, code);
}

inline void report(const std::string& tag,
                   const std::string& message,
                   int line = 0, int col = 0,
                   const std::string& hint = "") {
    auto& st = _ErrState::get();
    st.hasError() = true;

    std::cerr << BLD << RED << "ERROR" << RST
              << "[" << YLW << tag << RST << "]"
              << ": " << BLD << message << RST << std::endl;

    _printLoc(st.filename(), line, col);

    if (line > 0 && col > 0) {
        std::string sl = st.getLine(line);
        if (!sl.empty()) {
            for (auto& c : sl) if (c == '\t') c = ' ';
            int col_end = col;
            size_t p = col - 1;
            while (p < sl.size() && !isspace((unsigned char)sl[p]) && sl[p] != ';')
                { ++p; ++col_end; }
            _printSrcLine(line, col, col_end, "");
        }
    } else if (line > 0) {
        std::string sl = st.getLine(line);
        if (!sl.empty()) {
            for (auto& c : sl) if (c == '\t') c = ' ';
            std::cerr << DIM << "   |" << RST << std::endl;
            std::cerr << BLU << " " << line << BLU << " | " << RST << sl << std::endl;
            std::cerr << DIM << "   |" << RST << std::endl;
        }
    }

    if (!hint.empty()) {
        std::cerr << "  " << CYN << "= " << RST
                  << DIM << "help:" << RST << " " << hint << std::endl;
    }
    std::cerr << std::endl;
}

inline void reportSimple(const std::string& tag,
                         const std::string& message,
                         const std::string& hint = "") {
    auto& st = _ErrState::get();
    st.hasError() = true;

    std::cerr << BLD << RED << "ERROR" << RST
              << "[" << YLW << tag << RST << "]"
              << ": " << BLD << message << RST << std::endl;

    if (!hint.empty()) {
        std::cerr << "  " << CYN << "= " << RST
                  << DIM << "help:" << RST << " " << hint << std::endl;
    }
    std::cerr << std::endl;
}

// Runtime error with a call-stack traceback.
// trace[0] = outermost frame (main), last = innermost. offsets are optional
// bytecode offsets (per frame) matching the addresses printed by 'fox -d'.
inline void reportRuntimeError(const std::string& message,
                               const std::vector<std::string>& trace,
                               const std::vector<size_t>& offsets = {}) {
    auto& st = _ErrState::get();
    st.hasError() = true;

    std::cerr << BLD << RED << "ERROR" << RST
              << "[" << YLW << "RuntimeError" << RST << "]"
              << ": " << BLD << message << RST << std::endl;

    if (!trace.empty()) {
        std::cerr << DIM << "Stack trace:" << RST << std::endl;
        for (size_t i = trace.size(); i-- > 0;) {
            std::cerr << "  " << CYN << "at " << RST << trace[i];
            if (i < offsets.size()) {
                char buf[32];
                snprintf(buf, sizeof(buf), " (bytecode 0x%04zX)", offsets[i]);
                std::cerr << DIM << buf << RST;
            }
            std::cerr << std::endl;
        }
    }
    std::cerr << std::endl;
}

// Parse "Syntax error: L:C: msg" or "L:C: msg"
inline void reportParseError(const std::string& raw) {
    auto& st = _ErrState::get();
    st.hasError() = true;

    std::string rest = raw;
    std::string prefix = "Syntax error: ";
    if (rest.compare(0, prefix.size(), prefix) == 0)
        rest = rest.substr(prefix.size());

    int line = 0, col = 0;
    _parseLineCol(rest, line, col, rest);

    std::cerr << BLD << RED << "ERROR" << RST
              << "[" << YLW << "ParseError" << RST << "]"
              << ": " << BLD << rest << RST << std::endl;

    _printLoc(st.filename(), line, col);

    if (line > 0 && col > 0) {
        _printSrcLine(line, col, col + 1, "");
    }

    std::cerr << std::endl;
}

// Auto-detect exception format
inline void reportFromException(const std::string& tag, const std::string& msg) {
    std::string prefix = "Syntax error: ";
    if (msg.compare(0, prefix.size(), prefix) == 0) {
        reportParseError(msg);
        return;
    }

    int line = 0, col = 0;
    std::string rest;
    if (_parseLineCol(msg, line, col, rest)) {
        report(tag, rest, line, col);
    } else {
        reportSimple(tag, msg);
    }
}

inline bool hasError()  { return _ErrState::get().hasError(); }
inline void clearError() { _ErrState::get().hasError() = false; }

} // namespace ErrorReporter
