/** Copyright (c) 2013, Jonathan Bohren, all rights reserved. 
 * This software is released under the BSD 3-clause license, for the details of
 * this license, please see LICENSE.txt at the root of this repository. 
 */

#include <string>
#include <vector>
#include <iterator>

#include <rtt/os/startstop.h>

#include <ocl/DeploymentComponent.hpp>
#include <ocl/TaskBrowser.hpp>
#include <ocl/LoggingService.hpp>
#include <rtt/Logger.hpp>
#include <rtt/deployment/ComponentLoader.hpp>
#include <rtt/scripting/Scripting.hpp>
#include <rtt/plugin/PluginLoader.hpp>


int main(int argc, char** argv) {
  // Initialize Orocos
  __os_init(argc, argv);

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

  RTT::OperationCaller<bool(std::string)> exec_file = 
    deployer->provides("Lua")->getOperation("exec_file");

  RTT::OperationCaller<bool(std::string)> exec_str = 
    deployer->provides("Lua")->getOperation("exec_str");

  // Load rttlib for first-class operation support
  exec_str("require(\"rttlib\")");

  // Define convenience variables
  exec_str("\
gs = rtt.provides();\
tc = rtt.getTC();\
depl = tc;");

  for(int i=1; i<argc; i++) {
    // Run until end-of-args sentinel
    std::string arg(argv[i]);
    if(arg == "--") {
      break;
    }
    // Execute the given script file
    RTT::log(RTT::Info)<<"Loading lua script: "<<arg<<RTT::endlog();
    exec_file(arg);
  }

  // Interface it:
  OCL::TaskBrowser browse(deployer.get());
  browse.loop();

  return 0;
}
