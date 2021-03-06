// C/C++ File

// Author:   Alexandre Tea <alexandre.qtea@gmail.com>
// File:     /Users/alexandretea/Work/ddti/srcs/master/SlaveNode.hpp
// Purpose:  Node that will execute the tasks sent by the MasterNode
// Created:  2017-07-26 18:51:03
// Modified: 2017-08-23 23:35:34

#ifndef SLAVENODE_H
#define SLAVENODE_H

#include <mlpack/core.hpp>
#include "ANode.hpp"
#include "ddti.hpp"
#include "task.hpp"

namespace ddti {

template <typename TaskManager>
class SlaveNode : public ANode
{
    public:
        SlaveNode(utils::mpi::Communicator const& comm)
            : ANode(comm), _task_manager(comm) {}

        virtual ~SlaveNode() {}

        SlaveNode(SlaveNode const& other) = delete;
        SlaveNode&      operator=(SlaveNode const& other) = delete;

    public:
        virtual void
        init_cli(int ac, char** av)
        {
            char*   args[3] = { av[0], nullptr, nullptr };

            for (int i = 0; i < ac; ++i) {
                char* arg = av[i];

                if (std::strcmp(arg, "-v") == 0
                        or std::strcmp(arg, "--verbose") == 0) {
                    args[1] = arg;
                    break ;
                }
            }
            mlpack::CLI::ParseCommandLine(args[1] == nullptr ? 1 : 2, args);
        }

        virtual void
        run()
        {
            int    task_code;

            ddti::Logger << "Running with tasks: " + _task_manager.name();
            _comm.recv_broadcast(task_code, ANode::MasterRank);
            while (task_code != task::End) {
                _task_manager(task_code);
                _comm.recv_broadcast(task_code, ANode::MasterRank);
            }
            _comm.barrier();
        }

    protected:
        TaskManager _task_manager;
};

}


#endif /* end of include guard: SLAVENODE_H */
