#ifndef __RTT_ROSSERVICE_ROS_SERVICE_PROXY_H
#define __RTT_ROSSERVICE_ROS_SERVICE_PROXY_H

#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string.hpp>

#include <ros/ros.h>

#include <rtt/RTT.hpp>
#include <rtt/plugin/ServicePlugin.hpp>
#include <rtt/internal/GlobalService.hpp>

//! Abstract ROS Service Proxy
class ROSServiceProxyBase
{
public:
  ROSServiceProxyBase(const std::string &service_name) : service_name_(service_name) { }
  //! Get the name of the ROS service
  const std::string& getServiceName() const { return service_name_; }
private:
  //! ROS Service name (fully qualified)
  std::string service_name_;
};


//! Abstract ROS Service Proxy Server
class ROSServiceServerProxyBase : public ROSServiceProxyBase
{ 
public:
  ROSServiceServerProxyBase(const std::string &service_name) : ROSServiceProxyBase(service_name) { }
private:
  //! The underlying ROS service server
  ros::ServiceServer server_;
};

template<class ROS_SERVICE_T, class ROS_SERVICE_REQ_T, class ROS_SERVICE_RESP_T>
class ROSServiceServerProxy : public ROSServiceServerProxyBase 
{
public:
  //! Operation caller for a ROS service server proxy
  typedef RTT::OperationCaller<bool(ROS_SERVICE_REQ_T&, ROS_SERVICE_RESP_T&)> ProxyOperationCallerType;

  /** \brief Construct a ROS service server and associate it with an Orocos
   * task's required interface and operation caller.
   */
  ROSServiceServerProxy(const std::string &service_name) :
    ROSServiceServerProxyBase(service_name),
    proxy_operation_caller_("ROS_SERVICE_SERVER_PROXY")
  {
    // Construct the ROS service server
    ros::NodeHandle nh;
    server_ = nh.advertiseService(
        service_name, 
        &ROSServiceServerProxy<ROS_SERVICE_T, ROS_SERVICE_REQ_T, ROS_SERVICE_RESP_T>::ros_service_callback, 
        this);
  }

  //! Connect an RTT Operation to this ROS service server
  bool connect(RTT::TaskContext *owner, RTT::OperationInterfacePart* operation) {
    // Link the caller with the operation
    return proxy_operation_caller_->setImplementation(
        operation->getLocalOperation(),
        owner->engine());
  }

private:

  //! The Orocos operation caller which gets called 
  ProxyOperationCallerType proxy_operation_caller_;
  
  //! The callback called by the ROS service server when this service is invoked
  bool ros_service_callback(ROS_SERVICE_REQ_T& request, ROS_SERVICE_RESP_T& response) {
    return proxy_operation_caller_.ready() && proxy_operation_caller_(request, response);
  }
};


//! Abstract ROS Service Proxy Client
class ROSServiceClientProxyBase : public ROSServiceProxyBase
{
public:
  ROSServiceClientProxyBase(const std::string &service_name) : ROSServiceProxyBase(service_name) { }
private:
  //! The underlying ROS service client
  ros::ServiceClient client_;
};

template<class ROS_SERVICE_T, class ROS_SERVICE_REQ_T, class ROS_SERVICE_RESP_T>
class ROSServiceClientProxy : public ROSServiceClientProxyBase 
{
public:

  ROSServiceClientProxy(const std::string &service_name) :
    ROSServiceClientProxyBase(service_name),
    proxy_operation_("ROS_SERVICE_CLIENT_PROXY")
  {
    // Construct the underlying service client
    ros::NodeHandle nh;
    client_ = nh.serviceClient<ROS_SERVICE_T>(service_name);

    // Link the operation with the service client
    proxy_operation_.calls(
        &ROSServiceClientProxy<ROS_SERVICE_T,ROS_SERVICE_REQ_T,ROS_SERVICE_RESP_T>::orocos_operation_callback,
        this,
        RTT::ClientThread);
  }

  //! Connect an operation caller with this proxy
  bool connect(RTT::TaskContext *owner, RTT::base::OperationCallerBaseInvoker* operation_caller) {
    // Link the caller with the operation
    return operation_caller->setImplementation(
        proxy_operation_.getImplementation(),
        owner->engine());
  }

private:

  //! The RTT operation
  RTT::Operation<bool(ROS_SERVICE_REQ_T&, ROS_SERVICE_RESP_T&)> proxy_operation_;

  //! The callback for the RTT operation
  bool orocos_operation_callback(ROS_SERVICE_REQ_T& request, ROS_SERVICE_RESP_T& response) {
    return client_.exists() && client_.isValid() && client_.call(request, response);
  }
};



//! Abstract factory for ROS Service Proxy Factories
class ROSServiceProxyFactoryBase 
{
public:

  ROSServiceProxyFactoryBase(const std::string &service_type) :
    service_type_(service_type)
  { }

  //! Get the ROS service type
  const std::string& getType() { return service_type_; }

  //! Get a proxy to a ROS service client
  virtual ROSServiceClientProxyBase* create_client_proxy(const std::string &service_name) = 0;
  //! Get a proxy to a ROS service server
  virtual ROSServiceServerProxyBase* create_server_proxy(const std::string &service_name) = 0;

private:
  std::string service_type_;
};

template<class ROS_SERVICE_T, class ROS_SERVICE_REQ_T, class ROS_SERVICE_RESP_T>
class ROSServiceProxyFactory : public ROSServiceProxyFactoryBase 
{
public:

  ROSServiceProxyFactory(const std::string &service_type)
    : ROSServiceProxyFactoryBase(service_type)
  { }

  virtual ROSServiceClientProxyBase* create_client_proxy(const std::string &service_name) {
    return new ROSServiceClientProxy<ROS_SERVICE_T, ROS_SERVICE_REQ_T, ROS_SERVICE_RESP_T>(service_name);
  }

  virtual ROSServiceServerProxyBase* create_server_proxy( const std::string &service_name) {
    return new ROSServiceServerProxy<ROS_SERVICE_T, ROS_SERVICE_REQ_T, ROS_SERVICE_RESP_T>(service_name);
  }
};

#endif // ifndef __RTT_ROSSERVICE_ROS_SERVICE_PROXY_H