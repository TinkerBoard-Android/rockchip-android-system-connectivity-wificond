/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wificond/ap_interface_impl.h"

#include <android-base/logging.h>

#include "wificond/net/netlink_utils.h"

#include "wificond/ap_interface_binder.h"

using android::net::wifi::IApInterface;
using android::wifi_system::HostapdManager;
using android::wifi_system::InterfaceTool;
using std::string;
using std::unique_ptr;
using std::vector;

using EncryptionType = android::wifi_system::HostapdManager::EncryptionType;

namespace android {
namespace wificond {

ApInterfaceImpl::ApInterfaceImpl(const string& interface_name,
                                 uint32_t interface_index,
                                 NetlinkUtils* netlink_utils,
                                 InterfaceTool* if_tool,
                                 HostapdManager* hostapd_manager)
    : interface_name_(interface_name),
      interface_index_(interface_index),
      netlink_utils_(netlink_utils),
      if_tool_(if_tool),
      hostapd_manager_(hostapd_manager),
      binder_(new ApInterfaceBinder(this)) {
  // This log keeps compiler happy.
  LOG(DEBUG) << "Created ap interface " << interface_name_
             << " with index " << interface_index_;
}

ApInterfaceImpl::~ApInterfaceImpl() {
  binder_->NotifyImplDead();
  if_tool_->SetUpState(interface_name_.c_str(), false);
}

sp<IApInterface> ApInterfaceImpl::GetBinder() const {
  return binder_;
}

bool ApInterfaceImpl::StartHostapd() {
  return hostapd_manager_->StartHostapd();
}

bool ApInterfaceImpl::StopHostapd() {
  // Drop SIGKILL on hostapd.
  if (!hostapd_manager_->StopHostapd()) {
    // Logging was done internally.
    return false;
  }

  // Take down the interface.
  if (!if_tool_->SetUpState(interface_name_.c_str(), false)) {
    // Logging was done internally.
    return false;
  }

  // Since wificond SIGKILLs hostapd, hostapd has no chance to handle
  // the cleanup.
  // Besides taking down the interface, we also need to set the interface mode
  // back to station mode for the cleanup.
  if (!netlink_utils_->SetInterfaceMode(interface_index_,
                                        NetlinkUtils::STATION_MODE)) {
    LOG(ERROR) << "Failed to set interface back to station mode";
    return false;
  }

  return true;
}

bool ApInterfaceImpl::WriteHostapdConfig(const vector<uint8_t>& ssid,
                                         bool is_hidden,
                                         int32_t channel,
                                         EncryptionType encryption_type,
                                         const vector<uint8_t>& passphrase) {
  string config = hostapd_manager_->CreateHostapdConfig(
      interface_name_, ssid, is_hidden, channel, encryption_type, passphrase);

  if (config.empty()) {
    return false;
  }

  return hostapd_manager_->WriteHostapdConfig(config);
}

}  // namespace wificond
}  // namespace android
