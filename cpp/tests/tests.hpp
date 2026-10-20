#pragma once

#include <string>

#include "chess/Game.hpp"

// Each runner returns the number of failures (0 = pass). `dataDir` is docs/testdata.
int runBasicTests();
int runPerftTests(const std::string& dataDir, int maxDepth);
int runScenarioTests(const std::string& dataDir);
int runPgnTests(const std::string& dataDir);
int runReviewTests(const std::string& dataDir);
int runImportTests(const std::string& dataDir);
int runOpeningTests(const std::string& dataDir);
int runEngineTests(const std::string& dataDir);

// Builds a game from "startpos" or a FEN. Returns false if the FEN is invalid.
bool gameFromSpec(const std::string& spec, chess::Game& out);
const char* statusName(chess::GameStatus s);
