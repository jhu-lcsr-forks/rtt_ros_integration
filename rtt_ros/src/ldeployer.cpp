/** Copyright (c) 2013, Jonathan Bohren, all rights reserved.
 * This software is released under the BSD 3-clause license, for the details of
 * this license, please see LICENSE.txt at the root of this repository.
 */

#include <string>
#include <vector>
#include <iterator>
#include <cctype>

#include <rtt/os/startstop.h>

#include <ocl/DeploymentComponent.hpp>
#include <ocl/TaskBrowser.hpp>
#include <ocl/LoggingService.hpp>
#include <rtt/Logger.hpp>
#include <rtt/deployment/ComponentLoader.hpp>
#include <rtt/scripting/Scripting.hpp>
#include <rtt/plugin/PluginLoader.hpp>

#include <boost/program_options.hpp>


int main(int argc, char** argv) {
  // Initialize Orocos
  __os_init(argc, argv);

  // Count the number of args for this program
  unsigned my_argc = 0;
  for(; my_argc < argc; my_argc++) {
    // Run until end-of-args sentinel
    std::string arg(argv[my_argc]);
    if(arg == "--") {
      break;
    }
  }

  namespace po = boost::program_options;
  po::options_description desc("Options");

  desc.add_options()
    ("help,h", "Show program usage")
    ("log-level,l", po::value<std::string>(), "Level at which to log from RTT (case-insensitive) [Never, Fatal, Critical, Error, Warning, Info, Debug, Realtime]")
    ("start,s", po::value<std::string>(), "Lua deployment script file to run")
    ("define-globals,g", "Declare lua global variables like 'gs', 'tc', and 'depl'");

  po::variables_map vm;
  po::store(po::parse_command_line(my_argc, argv, desc), vm);
  po::notify(vm);

  if(vm.count("help")) {
    std::cout<<desc<<std::endl;
    return 1;
  }

  RTT::Logger::log().setStdStream(std::cerr);
  RTT::Logger::log().mayLogStdOut(true);
  RTT::Logger::log().setLogLevel(RTT::Logger::Warning);

  boost::shared_ptr<OCL::DeploymentComponent> deployer;

  deployer = boost::make_shared<OCL::DeploymentComponent>();

  // Load lua scripting service
  if(!RTT::plugin::PluginLoader::Instance()->loadService("Lua", deployer.get())) {
    RTT::log(RTT::Error) << "Could not load lua service." << RTT::endlog();
    return -1;
  }

  // Get script operations
  RTT::OperationCaller<bool(std::string)> exec_file =
    deployer->provides("Lua")->getOperation("exec_file");

  RTT::OperationCaller<bool(std::string)> exec_str =
    deployer->provides("Lua")->getOperation("exec_str");

  // Load rttlib for first-class operation support
  exec_str("require(\"rttlib\")");

  // Define convenience variables
  if(vm.count("define-globals")) {
    exec_str(
        "gs = rtt.provides();\
        tc = rtt.getTC();\
        depl = tc;");
  }

  if(vm.count("log-level")) {
    // Lean on lua to parse the log level string
    std::string log_level = vm["log-level"].as<std::string>();
    if(log_level.length() > 0) {
      log_level[0] = std::toupper(log_level[0]);
    }
    exec_str("rtt.setLogLevel(\""+log_level+"\");");
  }

  if(vm.count("start")) {
    // Execute the given script file
    std::string lua_file = vm["start"].as<std::string>();
    RTT::log(RTT::Info)<<"Loading lua script: "<<lua_file<<RTT::endlog();
    exec_file(lua_file);
    RTT::log(RTT::Info)<<"Completed executing lua script: "<<lua_file<<RTT::endlog();
  }

  // Interface it:
  OCL::TaskBrowser browse(deployer.get());
  browse.loop();

  return 0;
}
