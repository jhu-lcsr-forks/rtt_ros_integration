
#include <rtt_rosclock/rtt_Rosclock_sim_clock_activity.hpp>

#include <rtt/TaskContext.hpp>
#include <rtt/internal/GlobalService.hpp>
#include <rtt/plugin/Plugin.hpp>

#include <ros/node_handle.h>
#include <ros/param.h>
#include <ros/subscribe_options.h>

using namespace rtt_rosclock;

boost::weak_ptr<SimClockThread> SimClockThread::singleton;

boost::shared_ptr<SimClockThread> SimClockThread::GetInstance()
{
  return singleton.lock();
}

boost::shared_ptr<SimClockThread> SimClockThread::Instance()
{
  // Create a new singleton, if necessary
  boost::shared_ptr<SimClockThread> shared = GetInstance();
  if(singleton.expired()) {
    shared.reset(new SimClockThread());
    singleton = shared;
  }

  return shared;
}

SimClockThread::SimClockThread(const std::string& name, RTT::TaskContext* owner) 
  : RTT::Service(name, owner)
  , RTT::os::Thread(ORO_SCHED_OTHER, RTT::os::LowestPriority, 0.0, 0, name)
  , time_service_(RTT::os::TimeService::Instance())
  , clock_source_(SIM_CLOCK_SOURCE_MANUAL)
  , process_callbacks_(false)
{
}

bool SimClockThread::setClockSource(ClockSource clock_source) 
{
  // Don't allow changing the source while running
  if(this->isActive()) {
    RTT::log(RTT::Error) << "The SimClockThread clock source cannot be changed while the thread is running." << RTT::endlog();
    return false;
  }

  // Set the clock source 
  clock_source_ = clock_source;

  return true;
}

bool SimClockThread::useROSClockTopic() 
{
  return this->setClockSource(SIM_CLOCK_SOURCE_ROS_CLOCK_TOPIC);
}

bool SimClockThread::useManualClock() 
{
  return this->setClockSource(SIM_CLOCK_SOURCE_MANUAL);
}

bool SimClockThread::simTimeEnabled() const 
{ 
  return this->isActive();
}

void SimClockThread::clockMsgCallback(const rosgraph_msgs::ClockConstPtr& clock)
{
  // Get the simulation time
  using namespace RTT::os;
  TimeService::Seconds clock_secs =
    (TimeService::Seconds)clock->clock.sec +
    ((TimeService::Seconds)clock->clock.nsec)*1E-9;

  // Update the RTT clock
  updateClockInternal(clock_secs);
}

bool SimClockThread::updateClock(RTT::os::TimeService::Seconds clock_secs)
{
  if(clock_source_ != SIM_CLOCK_SOURCE_MANUAL) {
    RTT::log(RTT::Error) << "Cannot update simulation clock manually unless the clock source is set to MANUAL_CLOCK." << RTT::endlog();
    return false;
  }

  return this->updateClockInternal(clock_secs);
}

bool SimClockThread::updateClockInternal(RTT::os::TimeService::Seconds clock_secs)
{
  // Update the RTT time to match the gazebo time
  using namespace RTT::os;
  TimeService::ticks rtt_ticks = time_service_->getTicks();
  TimeService::Seconds rtt_secs = RTT::nsecs_to_Seconds(TimeService::ticks2nsecs(rtt_ticks));

  // Check if time restarted
  if(clock_secs == 0.0) {
    
    RTT::log(RTT::Warning) << "Time has reset to 0! Re-setting time service..." << RTT::endlog();

    // Re-set the time service and don't update the activities
    this->resetTimeService();

  } else {
    // Compute the time update
    TimeService::Seconds dt = clock_secs - rtt_secs;

    // Check if time went backwards
    if(dt < 0) {
      RTT::log(RTT::Warning) << "Time went backwards by " << dt << " seconds!" << RTT::endlog();
    }

    // Update the RTT clock
    time_service_->secondsChange(dt);

    // trigger all SimClockActivities
    boost::shared_ptr<SimClockActivityManager> manager = SimClockActivityManager::GetInstance();
    if (manager) {
      manager->setSimulationPeriod(dt);
      manager->update();
    }
  }

  return true;
}

void SimClockThread::resetTimeService()
{
  // We have to set the Logger reference time to zero in order to get correct logging timestamps.
  // RTT::Logger::Instance()->setReferenceTime(0);
  //
  // Unfortunately this method is not available, therefore shutdown and restart logging.
  // This workaround is not exact.

  // Shutdown the RTT Logger
  RTT::Logger::Instance()->shutdown();

  // Disable the RTT system clock so Gazebo can manipulate time and reset it to 0
  time_service_->enableSystemClock(false);
  time_service_->secondsChange(-time_service_->secondsSince(0));
  // assert(time_service_->getTicks() == 0);

  // Restart the RTT Logger with reference time 0
  RTT::Logger::Instance()->startup();
  // assert(RTT::Logger::Instance()->getReferenceTime() == 0)
}

bool SimClockThread::initialize()
{
  switch(clock_source_) {
    case SIM_CLOCK_SOURCE_ROS_CLOCK_TOPIC:
      // Get /use_sim_time parameter from ROS
      bool use_sim_time = false;
      ros::param::get("/use_sim_time", use_sim_time);

      if(!use_sim_time) {
        RTT::log(RTT::Info) << "Did not enable ROS simulation clock because the ROS parameter '/use_sim_time' is not set to true." << RTT::endlog();
        break_loop_ = true;
        return false;
      }

      RTT::log(RTT::Info) << "Switching to simulated time based on ROS /clock topic..." << RTT::endlog();

      // Reset the timeservice and logger
      this->resetTimeService();

      // Subscribe the /clock topic (simulation time, e.g. published by Gazebo)
      ros::SubscribeOptions ops = ros::SubscribeOptions::create<rosgraph_msgs::Clock>(
          "/clock", 1, boost::bind(&SimClockThread::clockMsgCallback, this, _1),
          ros::VoidConstPtr(), &callback_queue_);
      clock_subscriber_ = nh_.subscribe(ops);

      // The loop needs to run in order to call the callback queue
      process_callbacks_ = true;

      break;

    case SIM_CLOCK_SOURCE_MANUAL:

      RTT::log(RTT::Info) << "Switching to simulated time based on a manual clock source..." << RTT::endlog();

      // Reset the timeservice and logger
      this->resetTimeService();

      // We're not processing the callback queue, so we won't loop.
      process_callbacks_ = false;

      break;

    case default:

      RTT::log(RTT::Error) << "Unknown simulation clock source for SimClockThread!" << RTT::endlog();

      return false;
  };

  return true;
}

void SimClockThread::loop()
{
  static const ros::WallDuration timeout(0.1);

  // Service callbacks while 
  while(process_callbacks_) {
    callback_queue_.callAvailable(timeout);
  }
}

bool SimClockThread::breakLoop()
{
  process_callbacks_ = false;
  return true;
}

void SimClockThread::finalize()
{
  RTT::log(RTT::Info) << "Disabling simulated time..." << RTT::endlog();

  // Shutdown the subscriber so no more clock message events will be handled
  clock_subscriber_.shutdown();

  // Shutdown the RTT Logger
  RTT::Logger::Instance()->shutdown();

  // Re-enable system clock
  time_service->enableSystemClock(true);
  
  // Restart the RTT Logger with reference walltime
  RTT::Logger::Instance()->startup();
}
