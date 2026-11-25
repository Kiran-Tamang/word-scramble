// Word Scramble Game - OOP capstone (C++17)
// Build: g++ -std=c++17 -Wall -o word_scramble word_scramble.cpp
#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

// ---------- helpers ----------
std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return s;
}

// ---------- WordBank: owns the data ----------
class WordBank {
    std::map<std::string, std::vector<std::string>> words_;
public:
    WordBank() {
        words_["Animals"]   = {"ELEPHANT", "GIRAFFE", "DOLPHIN", "PENGUIN", "KANGAROO", "TIGER"};
        words_["Computing"] = {"COMPILER", "POINTER", "VARIABLE", "FUNCTION", "ALGORITHM", "STRING"};
        words_["Food"]      = {"KIMCHI", "NOODLES", "PANCAKE", "CHOCOLATE", "SANDWICH", "DUMPLING"};
    }
    std::vector<std::string> categories() const {
        std::vector<std::string> c;
        for (const auto& kv : words_) c.push_back(kv.first);
        return c;
    }
    const std::vector<std::string>& wordsIn(const std::string& cat) const {
        return words_.at(cat);
    }
};

// ---------- Scrambler: the core mechanism ----------
class Scrambler {
public:
    // Shuffle letters (Fisher-Yates via std::shuffle); retry if the result equals the original.
    static std::string scramble(const std::string& word, std::mt19937& rng) {
        std::string s = word;
        for (int i = 0; i < 50; ++i) {
            std::shuffle(s.begin(), s.end(), rng);
            if (s != word) break;
        }
        return s;
    }
};

// ---------- Round: state of one word ----------
struct Round {
    std::string answer, scrambled, category;
    int attemptsLeft = 0;
    int wrongGuesses = 0;
    bool hintUsed = false;
};

enum class GuessResult { Correct, Wrong, OutOfAttempts };

// ---------- Game: rules, attempts, scoring (no I/O, so it is reusable) ----------
class Game {
    std::vector<std::string> queue_;   // shuffled words for this session
    std::string category_;
    int maxAttempts_, score_ = 0, solved_ = 0, played_ = 0;
    Round round_;
    std::mt19937 rng_{std::random_device{}()};
public:
    Game(const WordBank& bank, const std::string& category, int rounds = 5, int maxAttempts = 3)
        : category_(category), maxAttempts_(maxAttempts) {
        queue_ = bank.wordsIn(category);
        std::shuffle(queue_.begin(), queue_.end(), rng_);
        if ((int)queue_.size() > rounds) queue_.resize(rounds);
    }
    bool nextRound() {
        if (queue_.empty()) return false;
        round_ = Round{};
        round_.answer = queue_.back(); queue_.pop_back();
        round_.scrambled = Scrambler::scramble(round_.answer, rng_);
        round_.category = category_;
        round_.attemptsLeft = maxAttempts_;
        ++played_;
        return true;
    }
    GuessResult guess(const std::string& g) {
        if (toUpper(g) == round_.answer) {
            int pts = (int)round_.answer.size() * 10 - round_.wrongGuesses * 5
                      - (round_.hintUsed ? 10 : 0);
            score_ += std::max(pts, 5);
            ++solved_;
            return GuessResult::Correct;
        }
        ++round_.wrongGuesses;
        return --round_.attemptsLeft > 0 ? GuessResult::Wrong : GuessResult::OutOfAttempts;
    }
    std::string hint() {                       // reveals first letter + length, costs points
        round_.hintUsed = true;
        return std::string(1, round_.answer[0]) + " (" + std::to_string(round_.answer.size()) + " letters)";
    }
    const Round& round() const { return round_; }
    int score() const { return score_; }
    int solved() const { return solved_; }
    int played() const { return played_; }
};

// ---------- Console front-end (skipped in the WebAssembly build) ----------
#ifndef __EMSCRIPTEN__
int main() {
    WordBank bank;
    auto cats = bank.categories();

    std::cout << "=== WORD SCRAMBLE ===\nChoose a category:\n";
    for (size_t i = 0; i < cats.size(); ++i) std::cout << "  " << i + 1 << ". " << cats[i] << "\n";
    size_t pick = 0;
    std::cout << "> ";
    std::cin >> pick;
    if (pick < 1 || pick > cats.size()) pick = 1;
    std::cin.ignore(1000, '\n');

    Game game(bank, cats[pick - 1]);
    while (game.nextRound()) {
        const Round& r = game.round();
        std::cout << "\n[" << r.category << "] Unscramble: " << r.scrambled
                  << "   (type 'hint' or 'skip')\n";
        while (true) {
            std::cout << "Attempts left " << r.attemptsLeft << " > ";
            std::string in;
            if (!std::getline(std::cin, in)) return 0;
            if (toUpper(in) == "HINT") { std::cout << "Hint: " << game.hint() << " (-10 pts)\n"; continue; }
            if (toUpper(in) == "SKIP") { std::cout << "Skipped. Answer: " << r.answer << "\n"; break; }
            GuessResult res = game.guess(in);
            if (res == GuessResult::Correct) { std::cout << "Correct! Score: " << game.score() << "\n"; break; }
            if (res == GuessResult::OutOfAttempts) { std::cout << "Out of attempts. Answer: " << r.answer << "\n"; break; }
            std::cout << "Wrong.\n";
        }
    }
    std::cout << "\nFinal score: " << game.score() << "  (" << game.solved() << "/" << game.played() << " solved)\n";
}
#endif

// ---------- WebAssembly bridge: exposes the same Game class to JavaScript ----------
#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#include <memory>

class WebGame {
    WordBank bank_;
    std::unique_ptr<Game> game_;
public:
    explicit WebGame(const std::string& category) : game_(new Game(bank_, category)) {}
    bool nextRound()              { return game_->nextRound(); }
    std::string scrambled() const { return game_->round().scrambled; }
    std::string answer() const    { return game_->round().answer; }
    int attemptsLeft() const      { return game_->round().attemptsLeft; }
    int guess(const std::string& g) { return (int)game_->guess(g); } // 0 correct, 1 wrong, 2 out of attempts
    std::string hint()            { return game_->hint(); }
    int score() const  { return game_->score(); }
    int solved() const { return game_->solved(); }
    int played() const { return game_->played(); }
};

std::string webCategories() {
    std::string out;
    for (const auto& c : WordBank().categories()) out += (out.empty() ? "" : ",") + c;
    return out;
}

EMSCRIPTEN_BINDINGS(word_scramble) {
    emscripten::function("categories", &webCategories);
    emscripten::class_<WebGame>("WebGame")
        .constructor<std::string>()
        .function("nextRound", &WebGame::nextRound)
        .function("scrambled", &WebGame::scrambled)
        .function("answer", &WebGame::answer)
        .function("attemptsLeft", &WebGame::attemptsLeft)
        .function("guess", &WebGame::guess)
        .function("hint", &WebGame::hint)
        .function("score", &WebGame::score)
        .function("solved", &WebGame::solved)
        .function("played", &WebGame::played);
}
#endif
