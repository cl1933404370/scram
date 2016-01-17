/*
 * Copyright (C) 2014-2016 Olzhas Rakhimov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "risk_analysis_tests.h"

#include <utility>

#include "error.h"

namespace scram {
namespace test {

TEST_F(RiskAnalysisTest, ProcessInput) {
  std::string tree_input = "./share/scram/input/fta/correct_tree_input.xml";
  ASSERT_NO_THROW(ProcessInputFile(tree_input));
  EXPECT_EQ(3, gates().size());
  EXPECT_EQ(1, gates().count("trainone"));
  EXPECT_EQ(1, gates().count("traintwo"));
  EXPECT_EQ(1, gates().count("topevent"));
  EXPECT_EQ(4, basic_events().size());
  EXPECT_EQ(1, basic_events().count("pumpone"));
  EXPECT_EQ(1, basic_events().count("pumptwo"));
  EXPECT_EQ(1, basic_events().count("valveone"));
  EXPECT_EQ(1, basic_events().count("valvetwo"));
  if (gates().count("topevent")) {
    GatePtr top = gates().at("topevent");
    EXPECT_EQ("topevent", top->id());
    ASSERT_NO_THROW(top->formula()->type());
    EXPECT_EQ("and", top->formula()->type());
    EXPECT_EQ(2, top->formula()->event_args().size());
  }
  if (gates().count("trainone")) {
    GatePtr inter = gates().at("trainone");
    EXPECT_EQ("trainone", inter->id());
    ASSERT_NO_THROW(inter->formula()->type());
    EXPECT_EQ("or", inter->formula()->type());
    EXPECT_EQ(2, inter->formula()->event_args().size());
  }
  if (basic_events().count("valveone")) {
    BasicEventPtr primary = basic_events().at("valveone");
    EXPECT_EQ("valveone", primary->id());
  }
}

// Test Probability Assignment
TEST_F(RiskAnalysisTest, PopulateProbabilities) {
  // Input with probabilities
  std::string tree_input =
      "./share/scram/input/fta/correct_tree_input_with_probs.xml";
  ASSERT_NO_THROW(ProcessInputFile(tree_input));
  ASSERT_EQ(4, basic_events().size());
  ASSERT_EQ(1, basic_events().count("pumpone"));
  ASSERT_EQ(1, basic_events().count("pumptwo"));
  ASSERT_EQ(1, basic_events().count("valveone"));
  ASSERT_EQ(1, basic_events().count("valvetwo"));
  ASSERT_NO_THROW(basic_events().at("pumpone")->p());
  ASSERT_NO_THROW(basic_events().at("pumptwo")->p());
  ASSERT_NO_THROW(basic_events().at("valveone")->p());
  ASSERT_NO_THROW(basic_events().at("valvetwo")->p());
  EXPECT_EQ(0.6, basic_events().at("pumpone")->p());
  EXPECT_EQ(0.7, basic_events().at("pumptwo")->p());
  EXPECT_EQ(0.4, basic_events().at("valveone")->p());
  EXPECT_EQ(0.5, basic_events().at("valvetwo")->p());
}

// Test Analysis of Two train system.
TEST_P(RiskAnalysisTest, AnalyzeDefault) {
  std::string tree_input = "./share/scram/input/fta/correct_tree_input.xml";
  ASSERT_NO_THROW(ProcessInputFile(tree_input));
  ASSERT_NO_THROW(ran->Analyze());
  std::set<std::set<std::string>> mcs = {{"pumpone", "pumptwo"},
                                         {"pumpone", "valvetwo"},
                                         {"pumptwo", "valveone"},
                                         {"valveone", "valvetwo"}};
  EXPECT_EQ(mcs, min_cut_sets());
  PrintCutSets();  // Quick visual verification and test.
}

TEST_P(RiskAnalysisTest, AnalyzeNonCoherentDefault) {
  std::string tree_input = "./share/scram/input/fta/correct_non_coherent.xml";
  ASSERT_NO_THROW(ProcessInputFile(tree_input));
  ASSERT_NO_THROW(ran->Analyze());
  std::set<std::set<std::string>> mcs = {{"pumpone", "pumptwo"},
                                         {"pumpone", "valvetwo"},
                                         {"valveone"}};
  EXPECT_EQ(mcs, min_cut_sets());
}

TEST_P(RiskAnalysisTest, AnalyzeWithProbability) {
  std::string with_prob =
      "./share/scram/input/fta/correct_tree_input_with_probs.xml";
  std::set<std::string> mcs_1 = {"pumpone", "pumptwo"};
  std::set<std::string> mcs_2 = {"pumpone", "valvetwo"};
  std::set<std::string> mcs_3 = {"pumptwo", "valveone"};
  std::set<std::string> mcs_4 = {"valveone", "valvetwo"};
  std::set<std::set<std::string>> mcs = {mcs_1, mcs_2, mcs_3, mcs_4};
  settings.probability_analysis(true);
  ASSERT_NO_THROW(ProcessInputFile(with_prob));
  ASSERT_NO_THROW(ran->Analyze());

  EXPECT_EQ(mcs, min_cut_sets());
  if (settings.approximation() == "rare-event") {
    EXPECT_DOUBLE_EQ(1, p_total());
  } else {
    EXPECT_DOUBLE_EQ(0.646, p_total());
  }
  EXPECT_DOUBLE_EQ(0.42, mcs_probability().at(mcs_1));
  EXPECT_DOUBLE_EQ(0.3, mcs_probability().at(mcs_2));
  EXPECT_DOUBLE_EQ(0.28, mcs_probability().at(mcs_3));
  EXPECT_DOUBLE_EQ(0.2, mcs_probability().at(mcs_4));
}

// Test for exact probability calculation
// regardless of the qualitative analysis algorithm.
TEST_P(RiskAnalysisTest, EnforceExactProbability) {
  std::string with_prob =
      "./share/scram/input/fta/correct_tree_input_with_probs.xml";
  settings.probability_analysis(true).approximation("no");
  ASSERT_NO_THROW(ProcessInputFile(with_prob));
  ASSERT_NO_THROW(ran->Analyze());
  EXPECT_DOUBLE_EQ(0.646, p_total());
}

TEST_P(RiskAnalysisTest, AnalyzeNestedFormula) {
  std::string nested_input = "./share/scram/input/fta/nested_formula.xml";
  std::set<std::set<std::string>> mcs = {{"pumpone", "pumptwo"},
                                         {"pumpone", "valvetwo"},
                                         {"pumptwo", "valveone"},
                                         {"valveone", "valvetwo"}};
  ASSERT_NO_THROW(ProcessInputFile(nested_input));
  ASSERT_NO_THROW(ran->Analyze());
  EXPECT_EQ(mcs, min_cut_sets());
}

TEST_F(RiskAnalysisTest, ImportanceDefault) {
  std::string with_prob =
      "./share/scram/input/fta/correct_tree_input_with_probs.xml";
  settings.importance_analysis(true);
  ASSERT_NO_THROW(ProcessInputFile(with_prob));
  ASSERT_NO_THROW(ran->Analyze());
  // Check importance values.
  std::vector<std::pair<std::string, ImportanceFactors>> importance = {
      {"pumpone", {0.51, 0.4737, 0.7895, 1.316, 1.9}},
      {"pumptwo", {0.38, 0.4118, 0.8235, 1.176, 1.7}},
      {"valveone", {0.34, 0.2105, 0.5263, 1.316, 1.267}},
      {"valvetwo", {0.228, 0.1765, 0.5882, 1.176, 1.214}}};

  for (const auto& entry : importance) {
    const ImportanceFactors& result = RiskAnalysisTest::importance(entry.first);
    const ImportanceFactors& test = entry.second;
    EXPECT_NEAR(test.mif, result.mif, 1e-3) << entry.first;
    EXPECT_NEAR(test.cif, result.cif, 1e-3) << entry.first;
    EXPECT_NEAR(test.dif, result.dif, 1e-3) << entry.first;
    EXPECT_NEAR(test.raw, result.raw, 1e-3) << entry.first;
    EXPECT_NEAR(test.rrw, result.rrw, 1e-3) << entry.first;
  }
}

TEST_F(RiskAnalysisTest, ImportanceNeg) {
  std::string tree_input = "./share/scram/input/fta/importance_neg_test.xml";
  settings.importance_analysis(true);
  ASSERT_NO_THROW(ProcessInputFile(tree_input));
  ASSERT_NO_THROW(ran->Analyze());
  EXPECT_NEAR(0.04459, p_total(), 1e-3);
  // Check importance values with negative event.
  std::vector<std::pair<std::string, ImportanceFactors>> importance = {
      {"pumpone", {0.0765, 0.1029, 0.1568, 2.613, 1.115}},
      {"pumptwo", {0.057, 0.08948, 0.1532, 2.189, 1.098}},
      {"valveone", {0.94, 0.8432, 0.8495, 21.237, 6.379}},
      {"valvetwo", {0.0558, 0.06257, 0.1094, 2.189, 1.067}}};

  for (const auto& entry : importance) {
    const ImportanceFactors& result = RiskAnalysisTest::importance(entry.first);
    const ImportanceFactors& test = entry.second;
    EXPECT_NEAR(test.mif, result.mif, 1e-3) << entry.first;
    EXPECT_NEAR(test.cif, result.cif, 1e-3) << entry.first;
    EXPECT_NEAR(test.dif, result.dif, 1e-3) << entry.first;
    EXPECT_NEAR(test.raw, result.raw, 1e-3) << entry.first;
    EXPECT_NEAR(test.rrw, result.rrw, 1e-3) << entry.first;
  }
}

// Apply the rare event approximation.
TEST_P(RiskAnalysisTest, ImportanceRareEvent) {
  std::string with_prob = "./share/scram/input/fta/importance_test.xml";
  // Probability calculations with the rare event approximation.
  settings.approximation("rare-event").importance_analysis(true);
  ASSERT_NO_THROW(ProcessInputFile(with_prob));
  ASSERT_NO_THROW(ran->Analyze());
  EXPECT_DOUBLE_EQ(0.012, p_total());  // Adjusted probability.
  // Check importance values.
  std::vector<std::pair<std::string, ImportanceFactors>> importance = {
      {"pumpone", {0.12, 0.6, 0.624, 10.4, 2.5}},
      {"pumptwo", {0.1, 0.5833, 0.6125, 8.75, 2.4}},
      {"valveone", {0.12, 0.4, 0.424, 10.6, 1.667}},
      {"valvetwo", {0.1, 0.4167, 0.4458, 8.917, 1.714}}};

  for (const auto& entry : importance) {
    const ImportanceFactors& result = RiskAnalysisTest::importance(entry.first);
    const ImportanceFactors& test = entry.second;
    EXPECT_NEAR(test.mif, result.mif, 1e-3) << entry.first;
    EXPECT_NEAR(test.cif, result.cif, 1e-3) << entry.first;
    EXPECT_NEAR(test.dif, result.dif, 1e-3) << entry.first;
    EXPECT_NEAR(test.raw, result.raw, 1e-3) << entry.first;
    EXPECT_NEAR(test.rrw, result.rrw, 1e-3) << entry.first;
  }
}

// Apply the minimal cut set upper bound approximation.
TEST_P(RiskAnalysisTest, MCUB) {
  std::string with_prob =
      "./share/scram/input/fta/correct_tree_input_with_probs.xml";
  // Probability calculations with the MCUB approximation.
  settings.approximation("mcub").importance_analysis(true);
  ASSERT_NO_THROW(ProcessInputFile(with_prob));
  ASSERT_NO_THROW(ran->Analyze());
  EXPECT_DOUBLE_EQ(0.766144, p_total());
}

// Apply the minimal cut set upper bound approximation for non-coherent tree.
// This should be a warning.
TEST_P(RiskAnalysisTest, McubNonCoherent) {
  std::string with_prob = "./share/scram/input/core/a_and_not_b.xml";
  // Probability calculations with the MCUB approximation.
  settings.approximation("mcub").probability_analysis(true);
  ASSERT_NO_THROW(ProcessInputFile(with_prob));
  ASSERT_NO_THROW(ran->Analyze());
  EXPECT_NEAR(0.10, p_total(), 1e-5);
}

// Test Monte Carlo Analysis
/// @todo Expand this test.
TEST_P(RiskAnalysisTest, AnalyzeMC) {
  settings.uncertainty_analysis(true);
  std::string tree_input =
      "./share/scram/input/fta/correct_tree_input_with_probs.xml";
  ASSERT_NO_THROW(ProcessInputFile(tree_input));
  ASSERT_NO_THROW(ran->Analyze());
}

// Test Reporting capabilities
// Tests the output against the schema. However the contents of the
// output are not verified or validated.
TEST_F(RiskAnalysisTest, ReportIOError) {
  std::string tree_input = "./share/scram/input/fta/correct_tree_input.xml";
  // Messing up the output file.
  std::string output = "abracadabra.cadabraabra/output.txt";
  ASSERT_NO_THROW(ProcessInputFile(tree_input));
  ASSERT_NO_THROW(ran->Analyze());
  EXPECT_THROW(ran->Report(output), IOError);
}

// Reporting of the default analysis for MCS only without probabilities.
TEST_P(RiskAnalysisTest, ReportDefaultMCS) {
  CheckReport("./share/scram/input/fta/correct_tree_input.xml");
}

// Reporting of analysis for MCS with probability results.
TEST_F(RiskAnalysisTest, ReportProbability) {
  std::string tree_input =
      "./share/scram/input/fta/correct_tree_input_with_probs.xml";
  settings.probability_analysis(true);
  CheckReport(tree_input);
}

// Reporting of importance analysis.
TEST_F(RiskAnalysisTest, ReportImportanceFactors) {
  std::string tree_input =
      "./share/scram/input/fta/correct_tree_input_with_probs.xml";
  settings.importance_analysis(true);
  CheckReport(tree_input);
}

// Reporting of uncertainty analysis.
TEST_F(RiskAnalysisTest, ReportUncertaintyResults) {
  std::string tree_input =
      "./share/scram/input/fta/correct_tree_input_with_probs.xml";
  settings.uncertainty_analysis(true);
  CheckReport(tree_input);
}

// Reporting of CCF analysis.
TEST_F(RiskAnalysisTest, ReportCCF) {
  std::string tree_input = "./share/scram/input/core/mgl_ccf.xml";
  settings.ccf_analysis(true).importance_analysis(true);
  CheckReport(tree_input);
}

// Reporting of Negative events in MCS.
TEST_F(RiskAnalysisTest, ReportNegativeEvent) {
  std::string tree_input = "./share/scram/input/core/a_or_not_b.xml";
  settings.probability_analysis(true);
  CheckReport(tree_input);
}

// Reporting of all possible analyses.
TEST_F(RiskAnalysisTest, ReportAll) {
  std::string tree_input =
      "./share/scram/input/fta/correct_tree_input_with_probs.xml";
  settings.importance_analysis(true).uncertainty_analysis(true)
      .ccf_analysis(true);
  CheckReport(tree_input);
}

// Reporting with public or private roles.
TEST_F(RiskAnalysisTest, ReportRoles) {
  std::string tree_input = "./share/scram/input/fta/mixed_roles.xml";
  CheckReport(tree_input);
}

// Reporting of orphan primary events.
TEST_F(RiskAnalysisTest, ReportOrphanPrimaryEvents) {
  std::string tree_input = "./share/scram/input/fta/orphan_primary_event.xml";
  CheckReport(tree_input);
}

// Reporting of unused parameters.
TEST_F(RiskAnalysisTest, ReportUnusedParameters) {
  std::string tree_input = "./share/scram/input/fta/unused_parameter.xml";
  CheckReport(tree_input);
}

// NAND and NOR as a child cases.
TEST_P(RiskAnalysisTest, ChildNandNorGates) {
  std::string tree_input = "./share/scram/input/fta/children_nand_nor.xml";
  ASSERT_NO_THROW(ProcessInputFile(tree_input));
  ASSERT_NO_THROW(ran->Analyze());
  /// @todo Enable for prime implicants.
  /* std::set<std::set<std::string>> mcs = { */
  /*     {"not pumpone", "not pumptwo", "not valveone"}, */
  /*     {"not pumpone", "not valvetwo", "not valveone"}}; */
  std::set<std::set<std::string>> mcs = {{}};
  EXPECT_EQ(mcs, min_cut_sets());
}

// Simple test for several house event propagation.
TEST_P(RiskAnalysisTest, ManyHouseEvents) {
  std::string tree_input = "./share/scram/input/fta/constant_propagation.xml";
  ASSERT_NO_THROW(ProcessInputFile(tree_input));
  ASSERT_NO_THROW(ran->Analyze());
  std::set<std::set<std::string>> mcs = {{"a", "b"}};
  EXPECT_EQ(mcs, min_cut_sets());
}

// Simple test for several constant gate propagation.
TEST_P(RiskAnalysisTest, ConstantGates) {
  std::string tree_input = "./share/scram/input/fta/constant_gates.xml";
  ASSERT_NO_THROW(ProcessInputFile(tree_input));
  ASSERT_NO_THROW(ran->Analyze());
  std::set<std::set<std::string>> mcs = {{}};
  EXPECT_EQ(mcs, min_cut_sets());
}

}  // namespace test
}  // namespace scram
