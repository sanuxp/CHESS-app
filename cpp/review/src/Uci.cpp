#include "chess/Uci.hpp"

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace chess {

std::optional<UciInfo> parseInfo(const std::string& line) {
    if (line.compare(0, 5, "info ") != 0) return std::nullopt;
    std::istringstream in(line);
    std::vector<std::string> t;
    for (std::string w; in >> w;) t.push_back(w);

    UciInfo info;
    bool haveScore = false;
    for (size_t i = 1; i < t.size(); ++i) {
        if (t[i] == "depth" && i + 1 < t.size()) {
            info.depth = std::atoi(t[++i].c_str());
        } else if (t[i] == "multipv" && i + 1 < t.size()) {
            info.multipv = std::atoi(t[++i].c_str());
        } else if (t[i] == "score" && i + 2 < t.size()) {
            const std::string& kind = t[i + 1];
            info.score = kind == "mate" ? EngineScore::Mate(std::atoi(t[i + 2].c_str()))
                                        : EngineScore::Cp(std::atoi(t[i + 2].c_str()));
            haveScore = true;
            i += 2;
            if (i + 1 < t.size() && (t[i + 1] == "lowerbound" || t[i + 1] == "upperbound")) return std::nullopt;
        } else if (t[i] == "pv") {
            info.pv.assign(t.begin() + static_cast<std::ptrdiff_t>(i) + 1, t.end());
            break;
        }
    }
    if (!haveScore || info.pv.empty()) return std::nullopt;
    return info;
}

std::optional<std::string> parseBestMove(const std::string& line) {
    if (line.compare(0, 8, "bestmove") != 0) return std::nullopt;
    std::istringstream in(line);
    std::string word, move;
    in >> word >> move;
    return move.empty() ? std::string("(none)") : move;
}

std::string engineSearchHint() {
    return "Stockfish was not found. Run tools\\get_stockfish.ps1 (it installs engine\\stockfish.exe), or put a "
           "Stockfish program in an \"engine\" folder next to the app.";
}

// ------------------------------------------------------------------ finding the engine

namespace {

bool fileExists(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    return access(path.c_str(), X_OK) == 0;
#endif
}

#ifdef _WIN32
const char* const kExeName = "stockfish.exe";
const char kSep = '\\';
#else
const char* const kExeName = "stockfish";
const char kSep = '/';
#endif

std::string parentOf(const std::string& dir) {
    size_t cut = dir.find_last_of("/\\");
    return cut == std::string::npos ? std::string() : dir.substr(0, cut);
}

std::string executableDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return parentOf(std::string(buf, n));
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    return n > 0 ? parentOf(std::string(buf, static_cast<size_t>(n))) : std::string();
#endif
}

}  // namespace

std::string findEnginePath(const std::string& customPath) {
    if (!customPath.empty() && fileExists(customPath)) return customPath;

    std::string dir = executableDir();
    for (int i = 0; i < 10 && !dir.empty(); ++i) {
        std::string candidate = dir + kSep + "engine" + kSep + kExeName;
        if (fileExists(candidate)) return candidate;
        dir = parentOf(dir);
    }
    for (const char* rel : {"engine", "../engine"}) {  // tests run from build/ or the repository
        std::string candidate = std::string(rel) + kSep + kExeName;
        if (fileExists(candidate)) return candidate;
    }
    if (const char* env = std::getenv("CHESS_STOCKFISH")) {
        if (fileExists(env)) return env;
    }
    return {};
}

// ------------------------------------------------------------------ child process transport

namespace {

class ProcessTransport : public UciTransport {
public:
    ~ProcessTransport() override { close(); }

    bool start(const std::string& path) {
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        HANDLE inRead = nullptr, inWrite = nullptr, outRead = nullptr, outWrite = nullptr;
        if (!CreatePipe(&outRead, &outWrite, &sa, 0) || !CreatePipe(&inRead, &inWrite, &sa, 0)) return false;
        SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = inRead;
        si.hStdOutput = outWrite;
        si.hStdError = outWrite;
        PROCESS_INFORMATION pi{};
        std::string cmd = "\"" + path + "\"";
        BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        CloseHandle(inRead);
        CloseHandle(outWrite);
        if (!ok) {
            CloseHandle(inWrite);
            CloseHandle(outRead);
            return false;
        }
        CloseHandle(pi.hThread);
        process_ = pi.hProcess;
        write_ = inWrite;
        read_ = outRead;
#else
        int toChild[2], fromChild[2];
        if (pipe(toChild) != 0 || pipe(fromChild) != 0) return false;
        pid_ = fork();
        if (pid_ < 0) return false;
        if (pid_ == 0) {
            dup2(toChild[0], STDIN_FILENO);
            dup2(fromChild[1], STDOUT_FILENO);
            dup2(fromChild[1], STDERR_FILENO);
            close(toChild[0]); close(toChild[1]); close(fromChild[0]); close(fromChild[1]);
            execl(path.c_str(), path.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        close(toChild[0]);
        close(fromChild[1]);
        writeFd_ = toChild[1];
        readFd_ = fromChild[0];
#endif
        reader_ = std::thread([this] { readLoop(); });
        return true;
    }

    void send(const std::string& line) override {
        std::string data = line + "\n";
#ifdef _WIN32
        DWORD written = 0;
        if (write_) WriteFile(write_, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
#else
        if (writeFd_ >= 0) { ssize_t r = write(writeFd_, data.data(), data.size()); (void)r; }
#endif
    }

    bool readLine(std::string& line, std::chrono::milliseconds timeout) override {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [&] { return !queue_.empty() || ended_; })) return false;
        if (queue_.empty()) return false;
        line = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    void close() override {
        if (closed_.exchange(true)) return;
        send("quit");
#ifdef _WIN32
        if (process_) {
            if (WaitForSingleObject(process_, 1500) == WAIT_TIMEOUT) TerminateProcess(process_, 0);
            CloseHandle(process_);
            process_ = nullptr;
        }
        if (write_) { CloseHandle(write_); write_ = nullptr; }
        if (read_) { CloseHandle(read_); read_ = nullptr; }
#else
        if (writeFd_ >= 0) { ::close(writeFd_); writeFd_ = -1; }
        if (pid_ > 0) {
            int status = 0;
            kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
            pid_ = -1;
        }
        if (readFd_ >= 0) { ::close(readFd_); readFd_ = -1; }
#endif
        if (reader_.joinable()) reader_.join();
    }

private:
    void push(std::string line) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(line));
        cv_.notify_all();
    }

    void readLoop() {
        std::string pending;
        char buf[4096];
        for (;;) {
#ifdef _WIN32
            DWORD n = 0;
            if (!read_ || !ReadFile(read_, buf, sizeof(buf), &n, nullptr) || n == 0) break;
#else
            ssize_t n = readFd_ >= 0 ? read(readFd_, buf, sizeof(buf)) : -1;
            if (n <= 0) break;
#endif
            pending.append(buf, static_cast<size_t>(n));
            size_t nl;
            while ((nl = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, nl);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                push(std::move(line));
                pending.erase(0, nl + 1);
            }
        }
        std::lock_guard<std::mutex> lock(mutex_);
        ended_ = true;
        cv_.notify_all();
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::string> queue_;
    bool ended_ = false;
    std::atomic<bool> closed_{false};
    std::thread reader_;
#ifdef _WIN32
    HANDLE process_ = nullptr, write_ = nullptr, read_ = nullptr;
#else
    pid_t pid_ = -1;
    int writeFd_ = -1, readFd_ = -1;
#endif
};

}  // namespace

std::unique_ptr<UciTransport> openProcessTransport(const std::string& path) {
    if (path.empty()) return nullptr;
    auto t = std::make_unique<ProcessTransport>();
    if (!t->start(path)) return nullptr;
    return t;
}

// ------------------------------------------------------------------ engine client

UciEngine::~UciEngine() {
    if (transport_) transport_->close();
}

void UciEngine::waitFor(const std::string& command, const std::string& answer, std::chrono::milliseconds timeout) {
    transport_->send(command);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::string line;
    for (;;) {
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0 || !transport_->readLine(line, left)) {
            throw EngineError("The engine did not answer \"" + command + "\" within " +
                              std::to_string(timeout.count() / 1000) + " s.");
        }
        if (line.compare(0, 8, "id name ") == 0) name_ = line.substr(8);
        if (line == answer) return;
    }
}

void UciEngine::start(std::chrono::milliseconds timeout) {
    waitFor("uci", "uciok", timeout);
    waitFor("isready", "readyok", timeout);
}

void UciEngine::setOption(const std::string& name, const std::string& value) {
    transport_->send("setoption name " + name + " value " + value);
}

std::vector<EngineLine> UciEngine::analyze(const std::string& fen, int depth, int multiPv,
                                           std::chrono::milliseconds timeout, const std::atomic<bool>* cancel) {
    if (multiPv != multiPv_) {
        setOption("MultiPV", std::to_string(multiPv));
        multiPv_ = multiPv;
    }
    transport_->send("position fen " + fen);
    transport_->send("go depth " + std::to_string(depth));

    std::map<int, UciInfo> best;  // multipv index -> deepest info seen
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool stopped = false;
    std::string line;
    for (;;) {
        if (!stopped && cancel && cancel->load()) {
            transport_->send("stop");
            stopped = true;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            transport_->send("stop");
            throw EngineError("The engine took longer than " + std::to_string(timeout.count() / 1000) +
                              " s for one position.");
        }
        if (!transport_->readLine(line, std::chrono::milliseconds(100))) continue;
        if (auto info = parseInfo(line)) {
            auto it = best.find(info->multipv);
            if (it == best.end() || info->depth >= it->second.depth) best[info->multipv] = *info;
            continue;
        }
        if (auto move = parseBestMove(line)) {
            if (*move == "(none)") return {};
            std::vector<EngineLine> lines;
            for (auto& kv : best) lines.push_back(EngineLine{kv.second.pv.front(), kv.second.score, kv.second.pv});
            return lines;
        }
    }
}

}  // namespace chess
