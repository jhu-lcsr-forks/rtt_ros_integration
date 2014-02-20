/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2014, Intermodalics BVBA
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Intermodalics BVBA nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

#include <rtt_dynamic_reconfigure/auto_config.h>
#include <rtt/Property.hpp>
#include <rtt/internal/DataSources.hpp>

#include <climits>
#include <cfloat>

#include <boost/thread/locks.hpp>

#include <dynamic_reconfigure/config_tools.h>

namespace rtt_dynamic_reconfigure {

using namespace dynamic_reconfigure;
//using dynamic_reconfigure::Config;
//using dynamic_reconfigure::ConfigDescription;
//using dynamic_reconfigure::ParamDescription;
//using dynamic_reconfigure::Group;
//using dynamic_reconfigure::ConfigTools;

AutoConfig::AutoConfig()
    : owner_(0)
{
}

AutoConfig::AutoConfig(RTT::TaskContext *owner)
    : owner_(owner)
{
}

AutoConfig::~AutoConfig()
{
}

// default type
template <typename T> struct PropertyTypeInfo {
    typedef std::string dynamic_reconfigure_type;
    static std::string getType() { return "str"; }
    static T getMin() { return std::numeric_limits<T>::lowest(); }
    static T getMax() { return std::numeric_limits<T>::max(); }
};

template <> struct PropertyTypeInfo<bool>
{
    typedef bool dynamic_reconfigure_type;
    static std::string getType() { return "bool"; }
    static bool getMin() { return false; }
    static bool getMax() { return true; }
};

template <> struct PropertyTypeInfo<int>
{
    typedef int dynamic_reconfigure_type;
    static std::string getType() { return "int"; }
    static int getMin() { return INT_MIN; }
    static int getMax() { return INT_MAX; }
};

template <> struct PropertyTypeInfo<unsigned int>
{
    typedef int dynamic_reconfigure_type;
    static std::string getType() { return "int"; }
    static int getMin() { return 0; }
    static int getMax() { return UINT_MAX; }
};

template <> struct PropertyTypeInfo<std::string>
{
    typedef std::string dynamic_reconfigure_type;
    static std::string getType() { return "str"; }
    static std::string getMin() { return ""; }
    static std::string getMax() { return ""; }
};

template <> struct PropertyTypeInfo<double>
{
    typedef double dynamic_reconfigure_type;
    static std::string getType() { return "double"; }
    static double getMin() { return -DBL_MAX; }
    static double getMax() { return  DBL_MAX; }
};

template <> struct PropertyTypeInfo<float>
{
    typedef double dynamic_reconfigure_type;
    static std::string getType() { return "double"; }
    static double getMin() { return -FLT_MAX; }
    static double getMax() { return  FLT_MAX; }
};

template <typename T>
bool propertyFromMessage(Config &msg, const RTT::base::PropertyBase *sample, AutoConfig &config, const std::string &param_name)
{
    const RTT::Property<T> *sample_prop = dynamic_cast<const RTT::Property<T> *>(sample);
    if (!sample_prop) return false;

    typename PropertyTypeInfo<T>::dynamic_reconfigure_type value;
    if (!ConfigTools::getParameter(msg, param_name, value)) return false;

    RTT::Property<T> *prop = config.getPropertyType<T>(sample->getName());
    if (!prop) {
        prop = sample_prop->create();
        config.ownProperty(prop);
    }
    prop->set(value);
    return true;
}

bool AutoConfig::__fromMessage__(Config &msg, const AutoConfig &sample)
{
    return __fromMessage__(msg, sample, prefix_);
}

bool AutoConfig::__fromMessage__(Config &msg, const AutoConfig &sample, const std::string &prefix)
{
    bool result = true;
    for(RTT::PropertyBag::const_iterator i = sample.begin(); i != sample.end(); ++i) {
        RTT::base::PropertyBase *pb = this->getProperty((*i)->getName());
        std::string param_name = prefix + (*i)->getName();

        // search parameter in Config message
        bool param_found = false;
        for(Config::_bools_type::const_iterator n = msg.bools.begin(); n != msg.bools.end(); ++n) {
            if (n->name == param_name) param_found = true;
        }
        for(Config::_ints_type::const_iterator n = msg.ints.begin(); n != msg.ints.end(); ++n) {
            if (n->name == param_name) param_found = true;
        }
        for(Config::_strs_type::const_iterator n = msg.strs.begin(); n != msg.strs.end(); ++n) {
            if (n->name == param_name) param_found = true;
        }
        for(Config::_doubles_type::const_iterator n = msg.doubles.begin(); n != msg.doubles.end(); ++n) {
            if (n->name == param_name) param_found = true;
        }
        if (!param_found) continue;

        // get parameter value from Config message
        if (
            propertyFromMessage<bool>(msg, *i, *this, param_name) ||
            propertyFromMessage<int>(msg, *i, *this, param_name) ||
            propertyFromMessage<unsigned int>(msg, *i, *this, param_name) ||
            propertyFromMessage<std::string>(msg, *i, *this, param_name) ||
            propertyFromMessage<double>(msg, *i, *this, param_name) ||
            propertyFromMessage<float>(msg, *i, *this, param_name)
           ) continue;

        // For sub groups, add a sub config to *this and recurse...
        const RTT::Property<AutoConfig> *sample_sub = dynamic_cast<const RTT::Property<AutoConfig> *>(*i);
        if (sample_sub) {
            RTT::Property<AutoConfig> *sub = this->getPropertyType<AutoConfig>((*i)->getName());
            if (!sub) {
                sub = sample_sub->create();
                sub->set(AutoConfig(owner_));
                this->ownProperty(sub);
            }

            if (!sub->set().__fromMessage__(msg, sample_sub->rvalue(), param_name + "__"))
                result = false;
        }

        result = false;
    }

    return result;
}

template <typename T>
bool propertyToMessage(Config &msg, const RTT::base::PropertyBase *pb, const std::string &_prefix)
{
    const RTT::Property<T> *prop = dynamic_cast<const RTT::Property<T> *>(pb);
    if (!prop) return false;

    typename PropertyTypeInfo<T>::dynamic_reconfigure_type value = prop->get();
    ConfigTools::appendParameter(msg, _prefix + pb->getName(), value);
    return true;
}

template <>
bool propertyToMessage<AutoConfig>(Config &msg, const RTT::base::PropertyBase *pb, const std::string &_prefix)
{
    const RTT::Property<AutoConfig> *prop = dynamic_cast<const RTT::Property<AutoConfig> *>(pb);
    if (!prop) return false;

    bool result = true;
    std::string prefix(_prefix);
    if (!pb->getName().empty()) prefix += pb->getName() + "__";

    for(RTT::PropertyBag::const_iterator i = prop->rvalue().begin(); i != prop->rvalue().end(); ++i) {
        if (propertyToMessage<bool>(msg, *i, prefix) ||
            propertyToMessage<int>(msg, *i, prefix) ||
            propertyToMessage<unsigned int>(msg, *i, prefix) ||
            propertyToMessage<std::string>(msg, *i, prefix) ||
            propertyToMessage<double>(msg, *i, prefix) ||
            propertyToMessage<float>(msg, *i, prefix) ||
            propertyToMessage<AutoConfig>(msg, *i, prefix)
           ) continue;

        result = false;
    }

    return result;
}

void AutoConfig::__toMessage__(Config &msg) const
{
    RTT::Property<AutoConfig> bag(std::string(), std::string(), new RTT::internal::ReferenceDataSource<AutoConfig>(const_cast<AutoConfig&>(*this)));
    propertyToMessage<AutoConfig>(msg, &bag, prefix_);
}

void AutoConfig::__toServer__(const ros::NodeHandle &nh) const
{

}

void AutoConfig::__fromServer__(const ros::NodeHandle &nh)
{

}

void AutoConfig::__clamp__(ServerType *server)
{
    const AutoConfig &min = server->getConfigMin();
    const AutoConfig &max = server->getConfigMax();

    // TODO: clamp values
}

uint32_t AutoConfig::__level__(const AutoConfig &config) const
{
    return 0;
}

template <typename T>
static bool getParamDescription(const RTT::base::PropertyBase *pb, const std::string &prefix, Group::_parameters_type& params, AutoConfig& dflt, AutoConfig& min, AutoConfig& max)
{
    const RTT::Property<T> *prop = dynamic_cast<const RTT::Property<T> *>(pb);
    if (!prop) return false;

    ParamDescription param;
    param.name = prefix + pb->getName();
    param.type = PropertyTypeInfo<T>::getType();
    param.description = pb->getDescription();
    params.push_back(param);

    // get current value as default
    if (!dflt.getProperty(pb->getName())) {
        RTT::Property<T> *dflt_prop = prop->create();
        dflt_prop->set(prop->get());
        dflt.ownProperty(dflt_prop);
    }

    // get minimum/maximum value
    if (!min.getProperty(pb->getName())) {
        RTT::Property<T> *min_prop = prop->create();
        min_prop->set(PropertyTypeInfo<T>::getMin());
        min.ownProperty(min_prop);
    }
    if (!max.getProperty(pb->getName())) {
        RTT::Property<T> *max_prop = prop->create();
        max_prop->set(PropertyTypeInfo<T>::getMax());
        max.ownProperty(max_prop);
    }

    return true;
}

static void getGroupDescription(RTT::TaskContext *owner, const RTT::PropertyBag *bag, const std::string &prefix, ConfigDescription& config_description, AutoConfig& dflt, AutoConfig& min, AutoConfig& max, const std::string &name, const std::string &type, int32_t parent, int32_t id)
{
    config_description.groups.push_back(Group());
    Group &group = config_description.groups.back();
    group.name = name;
    group.type = type;
    group.parent = parent;
    group.id = id;

    for(RTT::PropertyBag::const_iterator i = bag->begin(); i != bag->end(); ++i) {
        if (getParamDescription<bool>(*i, prefix, group.parameters, dflt, min, max) ||
            getParamDescription<int>(*i, prefix, group.parameters, dflt, min, max) ||
            getParamDescription<unsigned int>(*i, prefix, group.parameters, dflt, min, max) ||
            getParamDescription<std::string>(*i, prefix, group.parameters, dflt, min, max) ||
            getParamDescription<double>(*i, prefix, group.parameters, dflt, min, max) ||
            getParamDescription<float>(*i, prefix, group.parameters, dflt, min, max)
           ) continue;

        const RTT::Property<RTT::PropertyBag> *sub = dynamic_cast<RTT::Property<RTT::PropertyBag> *>(*i);
        if (sub) {
            RTT::Property<AutoConfig> *sub_dflt = dflt.getPropertyType<AutoConfig>(sub->getName());
            if (!sub_dflt) dflt.ownProperty(sub_dflt = new RTT::Property<AutoConfig>(sub->getName(), sub->getDescription(), AutoConfig(owner)));
            RTT::Property<AutoConfig> *sub_min  = dflt.getPropertyType<AutoConfig>(sub->getName());
            if (!sub_min)  dflt.ownProperty(sub_min = new RTT::Property<AutoConfig>(sub->getName(), sub->getDescription(), AutoConfig(owner)));
            RTT::Property<AutoConfig> *sub_max  = dflt.getPropertyType<AutoConfig>(sub->getName());
            if (!sub_max)  dflt.ownProperty(sub_max = new RTT::Property<AutoConfig>(sub->getName(), sub->getDescription(), AutoConfig(owner)));
            getGroupDescription(owner, &(sub->rvalue()), prefix + sub->getName() + "__", config_description, sub_dflt->value(), sub_min->value(), sub_max->value(), sub->getName(), "", group.id, ++id);
        }
    }
}

std::map<AutoConfig::ServerType *, AutoConfig::CachePtr> AutoConfig::cache_;
boost::shared_mutex AutoConfig::cache_mutex_;

void AutoConfig::buildCache(ServerType *server, RTT::TaskContext *owner)
{
    boost::upgrade_lock<boost::shared_mutex> upgrade_lock(cache_mutex_);
    if (upgrade_lock.owns_lock())
    {
        boost::upgrade_to_unique_lock<boost::shared_mutex> unique_lock(upgrade_lock);
        CachePtr& cache = cache_[server];
        if (!cache) cache.reset(new Cache());
        cache->description_message_.reset(new ConfigDescription);
        getGroupDescription(owner, owner->properties(), std::string(), *(cache->description_message_), cache->default_, cache->min_, cache->max_, "Default", "", 0, 0);
    }
}

dynamic_reconfigure::ConfigDescriptionPtr AutoConfig::__getDescriptionMessage__(ServerType *server)
{
    boost::shared_lock<boost::shared_mutex> lock(cache_mutex_);
    if (!cache_.count(server)) buildCache(server, server->getOwner());
    return cache_.at(server)->description_message_;
}

const AutoConfig &AutoConfig::__getDefault__(ServerType *server)
{
    boost::shared_lock<boost::shared_mutex> lock(cache_mutex_);
    if (!cache_.count(server)) buildCache(server, server->getOwner());
    return cache_.at(server)->default_;
}

const AutoConfig &AutoConfig::__getMax__(ServerType *server)
{
    boost::shared_lock<boost::shared_mutex> lock(cache_mutex_);
    if (!cache_.count(server)) buildCache(server, server->getOwner());
    return cache_.at(server)->max_;
}

const AutoConfig &AutoConfig::__getMin__(ServerType *server)
{
    boost::shared_lock<boost::shared_mutex> lock(cache_mutex_);
    if (!cache_.count(server)) buildCache(server, server->getOwner());
    return cache_.at(server)->min_;
}

void AutoConfig::__refreshDescription__(ServerType *server)
{
    buildCache(server, server->getOwner());
}

} // namespace rtt_dynamic_reconfigure

template class RTT::Property<rtt_dynamic_reconfigure::AutoConfig>;
