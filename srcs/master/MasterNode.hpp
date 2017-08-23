// C/C++ File

// Author:   Alexandre Tea <alexandre.qtea@gmail.com>
// File:     /Users/alexandretea/Work/ddti/srcs/master/MasterNode.hpp
// Purpose:  TODO (a one-line explanation)
// Created:  2017-07-26 18:51:03
// Modified: 2017-08-23 21:45:52

#ifndef MASTERNODE_H
#define MASTERNODE_H

#include <fstream>
#include <string>
#include <mlpack/core.hpp>
#include "ANode.hpp"
#include "DecisionTree.hpp"
#include "ddti.hpp"
#include "Dataset.hpp"
#include "Classifier.hpp"

namespace ddti {

// TODO network terminal to give instructions to node? could be in ANode
// so SlaveNode has it too; inherits from a TerminalCapabilities class?
template <typename InductionAlgo>
class MasterNode : public ANode
{
    public:
        MasterNode(utils::mpi::Communicator const& process)
            : ANode(process)
        {}

        virtual ~MasterNode() {}

        MasterNode(MasterNode const& other) = delete;
        MasterNode&     operator=(MasterNode const& other) = delete;

    public:
        virtual void
        init_cli(int ac, char** av)
        {
            using namespace mlpack;

            // Input parameters
            PARAM_STRING_IN_REQ(PARAM_TRAINING_SET,
                                "Path to the training dataset.", "i");
            // TODO specify allowed formats for datasets, in param help or program info
            PARAM_STRING_IN(
                PARAM_TEST_SET,
                "The dataset used to test the predictive accuracy of the "
                "generated model. If none is provided, the training set "
                "will be used.",
                "t", ""
            );
            PARAM_INT_IN(
                PARAM_LABELS_DIMENSION,
                "Index of the column containing the labels to predict "
                "(must be between 0 and N-1). If unspecified, the algorithm "
                "will use the last column of the dataset.",
                "l", -1
            );
            PARAM_VECTOR_IN(std::string, PARAM_ATTRIBUTES,
                            "List of attribute names, separated by spaces "
                            "(e.g. -a name lastname age).", "a");
            PARAM_INT_IN(
                PARAM_MINLEAFSIZE,
                "Minimum number of instances per leaf.",
                "m", 2
            );
            PARAM_FLAG(
                PARAM_OUTPUT_DEBUG,
                "Prints some debug information such as Information Gain Ratio "
                "values.",
                "d"
            );

            // Output parameters
            PARAM_DOUBLE_OUT(
                OUT_PREDICTIVE_ACC,
                "The predictive accuracy of the generated model, obtained using"
                " the provided test set, or the training set if no test set was"
                " specified."
            );
            PARAM_DOUBLE_OUT(
                OUT_INDUC_DURAT,
                "Time taken to build the decision tree."
            );
            PARAM_STRING_OUT(
                OUT_MODEL_FILE,
                "The path of the file where the model will be dumped. The only "
                "supported output format at the moment is .txt. If unset, the "
                "model will be printed on the standard output.",
                "o"
            );
            // TODO timerrrr

            CLI::ParseCommandLine(ac, av);

            _train_set_path = CLI::GetParam<std::string>(PARAM_TRAINING_SET);
            _labels_dim = CLI::GetParam<int>(PARAM_LABELS_DIMENSION);
            _test_set_path = CLI::GetParam<std::string>(PARAM_TEST_SET);
            _attr_names =
                CLI::GetParam<std::vector<std::string>>(PARAM_ATTRIBUTES);

            // set algorithm parameters
            _params.debug = CLI::HasParam(PARAM_OUTPUT_DEBUG);
            _params.min_leaf_size =
                CLI::GetParam<int>(PARAM_MINLEAFSIZE);
        }

        virtual void
        run()
        {
            using namespace mlpack;

            ddti::Logger << "Running";
            try {
                Dataset<double> training_set = load_data(_train_set_path);
                Classifier      classifier(std::move(train(training_set)));
                double          acc;

                // test predictive accuracy
                if (_test_set_path.empty()) {
                    ddti::Logger << "Test model using training set";
                    acc = classifier.test(training_set);
                } else {
                    Dataset<double> test_set = load_data(_test_set_path);

                    ddti::Logger << "Test model using " + _test_set_path;
                    acc = classifier.test(test_set);
                }

                // output variables
                CLI::GetParam<double>(OUT_PREDICTIVE_ACC) = acc;
                CLI::GetParam<double>(OUT_INDUC_DURAT) = _train_duration;
                if (CLI::HasParam(OUT_MODEL_FILE))
                    output_model(classifier, training_set);
                else
                    classifier.dump_model(training_set);
                // TODO output tree size / nb leaves
            } catch (std::exception const& e) {
                ddti::Logger.log(e.what(), mlpack::Log::Fatal);
            }
            _comm.barrier();
        }

    protected:
        void
        output_model(Classifier const& classifier,
                     Dataset<double> const& dataset) const
        {
            std::fstream    stream(
                mlpack::CLI::GetParam<std::string>(OUT_MODEL_FILE).c_str(),
                std::fstream::out
            );

            classifier.dump_model(dataset, stream);
        }

        Dataset<double>
        load_data(std::string const& path)
        {
            mlpack::data::DatasetInfo   data_info;
            arma::mat                   data;

            mlpack::data::Load(path, data, data_info, true);
            if (_labels_dim == -1)  // labels dimension is not set
                _labels_dim = data.n_rows - 1;
            return Dataset<double>(data, _labels_dim, _attr_names, &data_info);
        }

        std::unique_ptr<DecisionTree>
        train(Dataset<double> const& dataset)
        {
            std::unique_ptr<DecisionTree>   dt_root;
            utils::datetime::Timer          timer;
            InductionAlgo                   induction_algorithm(_comm);

            ddti::Logger << "Induction started with the following parameters: "
                            + _params.to_string();
            timer.start();
            dt_root = std::move(induction_algorithm(dataset, _params));
            timer.stop();
            ddti::Logger << "Induction complete";
            _train_duration = timer.get<double>();
            return dt_root;
        }

    protected:
        std::string                 _train_set_path;
        int                         _labels_dim;
        std::string                 _test_set_path;
        std::vector<std::string>    _attr_names;
        double                      _train_duration;
        typename
        InductionAlgo::Parameters   _params;

        // Input parameter names
        static const char* PARAM_TRAINING_SET;
        static const char* PARAM_TEST_SET;
        static const char* PARAM_LABELS_DIMENSION;
        static const char* PARAM_ATTRIBUTES;
        static const char* PARAM_OUTPUT_DEBUG;
        static const char* PARAM_MINLEAFSIZE;

        // Output parameter names
        static const char* OUT_PREDICTIVE_ACC;
        static const char* OUT_MODEL_FILE;
        static const char* OUT_INDUC_DURAT; // induction duration
};

// Input parameter names
template <typename T>
const char* MasterNode<T>::PARAM_TRAINING_SET       = "training_set";
template <typename T>
const char* MasterNode<T>::PARAM_TEST_SET           = "test_set";
template <typename T>
const char* MasterNode<T>::PARAM_LABELS_DIMENSION   = "labels_column";
template <typename T>
const char* MasterNode<T>::PARAM_ATTRIBUTES         = "attributes";
template <typename T>
const char* MasterNode<T>::PARAM_OUTPUT_DEBUG       = "debug";
template <typename T>
const char* MasterNode<T>::PARAM_MINLEAFSIZE        = "min_leaf_size";

// Output parameter names
template <typename T>
const char* MasterNode<T>::OUT_MODEL_FILE           = "model_file";
template <typename T>
const char* MasterNode<T>::OUT_PREDICTIVE_ACC       = "Predictive accuracy";
template <typename T>
const char* MasterNode<T>::OUT_INDUC_DURAT          = "Induction duration (s)";

}   // end of namespace ddti


#endif /* end of include guard: MASTERNODE_H */
