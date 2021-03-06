// -*- C++ -*-
/*!
 * @file  ModifiedServo.cpp
 * @brief ModifiedServo component
 * $Date$ 
 *
 * $Id$ 
 */

#include "ModifiedServo.h"
// #include <iostream>  // Added by Rafa

// Module specification
// <rtc-template block="module_spec">
static const char* modifiedservo_spec[] =
  {
    "implementation_id", "ModifiedServo",
    "type_name",         "ModifiedServo",
    "description",       "ModifiedServo component",
    "version",           HRPSYS_PACKAGE_VERSION,
    "vendor",            "AIST",
    "category",          "example",
    "activity_type",     "DataFlowComponent",
    "max_instance",      "10",
    "language",          "C++",
    "lang_type",         "compile",
    // Configuration variables
    "conf.default.pdgains_sim_file_name", "",
    "conf.default.debugLevel", "0",
    ""
  };
// </rtc-template>

ModifiedServo::ModifiedServo(RTC::Manager* manager)
    // <rtc-template block="initializer">
  : RTC::DataFlowComponentBase(manager),
    m_tauRefIn("tauRef", m_tauRef),
    m_qRefIn("qRef", m_qRef),
    m_qIn("q", m_q),
    m_torqueModeIn("torqueMode", m_torqueMode),
    m_tauOut("tau", m_tau),
    // </rtc-template>
    gain_fname(""),
    dt(0.005),
    dof(0), loop(0),
    dummy(0)
{
}

ModifiedServo::~ModifiedServo()
{
}


RTC::ReturnCode_t ModifiedServo::onInitialize()
{
  // Registration: InPort/OutPort/Service
  // <rtc-template block="registration">
  // Set InPort buffers
  addInPort("tauRef", m_tauRefIn);
  addInPort("qRef", m_qRefIn);
  addInPort("q", m_qIn);
  addInPort("torqueMode", m_torqueModeIn);

  // Set OutPort buffer
  addOutPort("tau", m_tauOut);

  // Set service provider to Ports

  // Set service consumers to Ports

  // Set CORBA Service Ports

  // </rtc-template>

  // <rtc-template block="bind_config">
  // Bind variables and configuration variable

  bindParameter("pdgains_sim_file_name", gain_fname, "");
  bindParameter("debugLevel", m_debugLevel, "0");
  
  // </rtc-template>

  std::cout << m_profile.instance_name << ": onInitialize() " << std::endl;

  RTC::Properties& prop = getProperties();
  coil::stringTo(dt, prop["dt"].c_str());
  ref_dt = dt;
  coil::stringTo(ref_dt, prop["ref_dt"].c_str());
  nstep = ref_dt/dt;
  step = nstep;

  m_robot = hrp::BodyPtr(new hrp::Body());

  RTC::Manager& rtcManager = RTC::Manager::instance();
  std::string nameServer = rtcManager.getConfig()["corba.nameservers"];
  int comPos = nameServer.find(",");

  if (comPos < 0)
      comPos = nameServer.length();
  
  nameServer = nameServer.substr(0, comPos);
  RTC::CorbaNaming naming(rtcManager.getORB(), nameServer.c_str());
  
  if (!loadBodyFromModelLoader(m_robot, prop["model"].c_str(), 
                               CosNaming::NamingContext::_duplicate(naming.getRootContext())))
      std::cerr << "[" << m_profile.instance_name << "] failed to load model[" << prop["model"]
                << "]" << std::endl;
  
  return RTC::RTC_OK;
}

/*
RTC::ReturnCode_t ModifiedServo::onFinalize()
{
  return RTC::RTC_OK;
}
*/
/*
RTC::ReturnCode_t ModifiedServo::onStartup(RTC::UniqueId ec_id)
{
  return RTC::RTC_OK;
}
*/
/*
RTC::ReturnCode_t ModifiedServo::onShutdown(RTC::UniqueId ec_id)
{
  return RTC::RTC_OK;
}
*/

RTC::ReturnCode_t ModifiedServo::onActivated(RTC::UniqueId ec_id)
{
  std::cout << m_profile.instance_name << ": on Activated" << std::endl;

  if (m_qIn.isNew()) {
    m_qIn.read();
    if (dof == 0) {
      dof = m_q.data.length();
      readGainFile();
    }
  }
  
  return RTC::RTC_OK;
}

RTC::ReturnCode_t ModifiedServo::onDeactivated(RTC::UniqueId ec_id)
{
  std::cout << m_profile.instance_name << ": on Deactivated" << std::endl;
  
  return RTC::RTC_OK;
}

RTC::ReturnCode_t ModifiedServo::onExecute(RTC::UniqueId ec_id)
{
  loop++;
  
  if (m_tauRefIn.isNew())
    m_tauRefIn.read();
  
  if (m_qIn.isNew()) {
    m_qIn.read();
    if (dof == 0) {
        dof = m_q.data.length();
        readGainFile();
    }
  }

  if (m_qRefIn.isNew()) {
    m_qRefIn.read();
    step = nstep;
  }

  if (m_torqueModeIn.isNew())
    m_torqueModeIn.read();

  for (size_t i = 0; i < dof; i++) {
    
    double q = m_q.data[i];
    double qRef = step > 0 ? qRef_old[i] + (m_qRef.data[i] - qRef_old[i]) / step : qRef_old[i];

    double dq = (q - q_old[i]) / dt;
    double dqRef = (qRef - qRef_old[i]) / dt;

    q_old[i] = q;
    qRef_old[i] = qRef;

    double tau = m_torqueMode.data[i] ? m_tauRef.data[i] : Pgain[i] * (qRef - q) + Dgain[i] * (dqRef - dq);

    double tau_limit;
    
    if (m_robot && m_robot->numJoints() == dof)
        
        tau_limit = m_robot->joint(i)->climit * m_robot->joint(i)->gearRatio * m_robot->joint(i)->torqueConst * tau_limit_ratio[i];
    
    else {
        
        tau_limit = (std::numeric_limits<double>::max)() * tau_limit_ratio[i];

        if (i == 0 && loop % 500 == 0)
            std::cerr << "[" << m_profile.instance_name << "] m_robot is not set properly!! Maybe ModelLoader is missing?" << std::endl;
    }
    
    if (loop % 100 == 0 && m_debugLevel == 1)
        std::cerr << "[" << m_profile.instance_name << "] joint = "
                  << i << ", tau = " << tau << ", q , qRef = (" << q << ", " << qRef
                  << "), dq, dqRef = (" << dq << ", " << dqRef << "), pd = ("
                  << Pgain[i] << ", " << Dgain[i] << "), tau_limit = " << tau_limit << std::endl;
    
    m_tau.data[i] = std::max(std::min(tau, tau_limit), -tau_limit);
  }

  step--;

  m_tau.tm = m_q.tm;
  m_tauOut.write();
  
  return RTC::RTC_OK;
}

/*
RTC::ReturnCode_t ModifiedServo::onAborting(RTC::UniqueId ec_id)
{
  return RTC::RTC_OK;
}
*/
/*
RTC::ReturnCode_t ModifiedServo::onError(RTC::UniqueId ec_id)
{
  return RTC::RTC_OK;
}
*/
/*
RTC::ReturnCode_t ModifiedServo::onReset(RTC::UniqueId ec_id)
{
  return RTC::RTC_OK;
}
*/
/*
RTC::ReturnCode_t ModifiedServo::onStateUpdate(RTC::UniqueId ec_id)
{
  return RTC::RTC_OK;
}
*/
/*
RTC::ReturnCode_t ModifiedServo::onRateChanged(RTC::UniqueId ec_id)
{
  return RTC::RTC_OK;
}
*/

void ModifiedServo::readGainFile()
{
  if (gain_fname == "") {
    RTC::Properties & prop = getProperties();
    coil::stringTo(gain_fname, prop["pdgains_sim_file_name"].c_str());
  }

  // initialize length of vectors

  q_old.resize(dof);
  qRef_old.resize(dof);

  m_tauRef.data.length(dof);
  m_qRef.data.length(dof);
  m_torqueMode.data.length(dof);

  tau_limit_ratio.resize(dof);
  
  Pgain.resize(dof);
  Dgain.resize(dof);
  
  gain.open(gain_fname.c_str());

  if (gain.is_open()) {

    double val;

    for (unsigned int i = 0; i < dof; i++) {

      if (gain >> val)
        Pgain[i] = val;
      else
        std::cerr << "[" << m_profile.instance_name << "] Gain file [" << gain_fname << "] is too short" << std::endl;

      if (gain >> val)
        Dgain[i] = val;
      else
        std::cerr << "[" << m_profile.instance_name << "] Gain file [" << gain_fname << "] is too short" << std::endl;
    }

    gain.close();

    std::cerr << "[" << m_profile.instance_name << "] Gain file [" << gain_fname << "] successfully read" << std::endl;
  }
  else
    std::cerr << "[" << m_profile.instance_name << "] Gain file [" << gain_fname << "] could not be opened" << std::endl;
  
  // tau_limit_ratio initialize
  RTC::Properties& prop = getProperties();
  
  if (prop["pdcontrol_tau_limit_ratio"] != "") {

      coil::vstring tau_limit_ratio_str = coil::split(prop["pdcontrol_tau_limit_ratio"], ",");
      
      if (tau_limit_ratio_str.size() == dof) {
          
          for (size_t i = 0; i < dof; i++)
              coil::stringTo(tau_limit_ratio[i], tau_limit_ratio_str[i].c_str());
          
          std::cerr << "[" << m_profile.instance_name << "] tau_limit_ratio is set to " << prop["pdcontrol_tau_limit_ratio"] << std::endl;
      }
      else {
          
          for (size_t i = 0; i < dof; i++)
              tau_limit_ratio[i] = 1.0;
          
          std::cerr << "[" << m_profile.instance_name << "] pdcontrol_tau_limit_ratio found, but invalid length (" << tau_limit_ratio_str.size() << " != " << dof << ")." << std::endl;
          std::cerr << "[" << m_profile.instance_name << "] All tau_limit_ratio are set to 1.0." << std::endl;
      }
  }
  else {
      for (size_t i = 0; i < dof; i++)
          tau_limit_ratio[i] = 1.0;
      
      std::cerr << "[" << m_profile.instance_name << "] No pdcontrol_tau_limit_ratio found." << std::endl;
      std::cerr << "[" << m_profile.instance_name << "] All tau_limit_ratio are set to 1.0." << std::endl;
  }

  // initialize angleRef, old_ref and old with angle
  for(unsigned int i=0; i < dof; ++i) {
    m_tauRef.data[i] = 0.0;
    m_qRef.data[i] = qRef_old[i] = q_old[i] = m_q.data[i];
    m_torqueMode.data[i] = false;
  }
      
  // Print loaded gain
  std::cerr << "[" << m_profile.instance_name << "] loadGain" << std::endl;
  for (unsigned int i=0; i<m_robot->numJoints(); i++)
      std::cerr << "[" << m_profile.instance_name << "]   " << m_robot->joint(i)->name << ", pgain = " << Pgain[i] << ", dgain = " << Dgain[i] << std::endl;
}

extern "C"
{
 
  void ModifiedServoInit(RTC::Manager* manager)
  {
    coil::Properties profile(modifiedservo_spec);
    manager->registerFactory(profile,
                             RTC::Create<ModifiedServo>,
                             RTC::Delete<ModifiedServo>);
  }
  
};
