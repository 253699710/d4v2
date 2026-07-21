/*
 * InteractiveCounter.cpp
 *
 * Interaktiver d4v2 Counter: haelt den Solver-State (Cache, Lemmas) im RAM
 * zwischen beliebig vielen count()-Calls mit verschiedenen Assumptions.
 *
 * Protokoll (stdin/stdout zeilenweise):
 *   Input:  "count [lit] [lit] ...\n"
 *           "+5"  = var 5 = True  (include)
 *           "-5"  = var 5 = False (exclude)
 *           leeres "count" = keine Assumptions (Gesamtzahl)
 *           "quit\n" = beenden
 *
 *   Output: "<Zahl>\n"  Anzahl Modelle unter den Assumptions
 *           "0\n"       UNSAT
 *           "error\n"   Protokollfehler
 *
 * Aufruf:
 *   ./interactive_counter -i formula.cnf
 */

#include <signal.h>
#include <boost/multiprecision/gmp.hpp>
#include <boost/program_options.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ParseOption.hpp"
#include "src/configurations/ConfigurationDpllStyleMethod.hpp"
#include "src/methods/DpllStyleMethod.hpp"
#include "src/methods/MethodManager.hpp"
#include "src/options/preprocs/OptionPreprocManager.hpp"
#include "src/preprocs/PreprocManager.hpp"
#include "src/problem/ProblemManager.hpp"

using namespace d4;
namespace po = boost::program_options;

MethodManager *methodRun = nullptr;

static void signalHandler(int signum) {
  if (methodRun != nullptr) methodRun->interrupt();
  exit(signum);
}

int main(int argc, char **argv) {
  po::options_description desc{"Options"};
  desc.add_options()
#include "option.dsc"
      ;

  signal(SIGINT, signalHandler);
  po::variables_map vm;
  po::store(parse_command_line(argc, argv, desc), vm);

  try {
    po::notify(vm);
  } catch (const po::error &ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }

  if (!vm.count("input")) {
    std::cerr << "Usage: " << argv[0] << " -i formula.cnf\n";
    return 1;
  }

  // Problem laden (alle Logs auf stderr damit stdout sauber bleibt)
  ProblemManager *initProblem = ProblemManager::makeProblemManager(
      vm["input"].as<std::string>(),
      ProblemInputTypeManager::getInputType(vm["input-type"].as<std::string>()),
      std::cerr);
  assert(initProblem);

  // Preprocessing
  ConfigurationPeproc configPreproc = parsePreprocConfiguration(vm);
  configPreproc.inputType = initProblem->getProblemType();
  ProblemManager *problem =
      MethodManager::runPreproc(configPreproc, initProblem, std::cerr);

  // Alle Variablen sammeln (fuer count()-Aufrufe benoetigt)
  std::vector<Var> allVars;
  for (unsigned i = 1; i <= problem->getNbVar(); i++)
    allVars.push_back(i);

  // Counter konfigurieren
  ConfigurationDpllStyleMethod config;
  config.methodName = MethodNameManager::getMethodName("counting");
  config.inputName = vm["input"].as<std::string>();
  config.problemInputType =
      ProblemInputTypeManager::getInputType(vm["input-type"].as<std::string>());
  config.cache = parseCacheConfiguration(vm);
  config.branchingHeuristic = parseBranchingHeuristicConfiguration(vm);
  config.solver.solverName =
      SolverNameManager::getSolverName(vm["solver"].as<std::string>());
  config.spec.specUpdateType =
      SpecUpdateManager::getSpecUpdate(vm["occurrence-manager"].as<std::string>());
  config.operationType = OperationTypeManager::getOperatorType("counting");
  config.exploitModel = vm["exploit-model-activated"].as<bool>();

  OptionDpllStyleMethod options(config);

  // Counter einmalig erstellen -- Cache und Solver-State bleiben
  // fuer alle folgenden count()-Calls im RAM erhalten
  // Logs auf stderr damit stdout sauber fuer das Protokoll bleibt
  DpllStyleMethod<mpz::mpz_int, mpz::mpz_int> *counter =
      new DpllStyleMethod<mpz::mpz_int, mpz::mpz_int>(options, problem,
                                                       std::cerr);
  methodRun = counter;

  // Bereit-Signal
  std::cout << "ready\n";
  std::cout.flush();

  // Interaktive Schleife
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "quit") break;
    if (line.empty()) continue;

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd != "count") {
      std::cout << "error\n";
      std::cout.flush();
      continue;
    }

    // Assumptions parsen: +5 = var5=True, -5 = var5=False
    std::vector<Lit> assumptions;
    std::string tok;
    bool parseError = false;
    while (iss >> tok) {
      try {
        int lit = std::stoi(tok);
        if (lit > 0)
          assumptions.push_back(Lit::makeLit(lit, false));  // positiv = True
        else if (lit < 0)
          assumptions.push_back(Lit::makeLit(-lit, true));  // negativ = False
      } catch (...) {
        parseError = true;
        break;
      }
    }

    if (parseError) {
      std::cout << "error\n";
      std::cout.flush();
      continue;
    }

    // count() benutzt den internen Cache -- viel schneller als run()
    // weil gelernte Lemmata und Cache-Eintraege erhalten bleiben
    mpz::mpz_int result = counter->count(allVars, assumptions, std::cout);

    std::cout << result << "\n";
    std::cout.flush();
  }

  methodRun = nullptr;
  delete counter;
  delete initProblem;
  return 0;
}
