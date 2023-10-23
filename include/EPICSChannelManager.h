// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * EPICSSubscriptionManager.h
 *
 *  Created on: Mar 23, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "EPICSTypes.h"

#include <ChimeraTK/Exception.h>

#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

namespace ChimeraTK {
  class EpicsBackendRegisterAccessorBase;
  class EpicsBackend;
  class ChannelManager;

  struct ChannelInfo {
    std::deque<EpicsBackendRegisterAccessorBase*> _accessors;
    bool _configured{false};
    bool connected{false};
    evid* _subscriptionId{nullptr}; ///< Id used for subscriptions
    bool _asyncReadActivated{false};
    //\ToDo: Use pointer to have name persistent
    std::shared_ptr<pv> _pv;
    std::string _caName;

    /**
     * Constructor.
     *
     * \param channelName
     * \throw ChimeraTK::runtime_error in case the channel access connection could not be set up.
     */
    ChannelInfo(std::string channelName);

    bool isChannelName(std::string channelName);

    bool operator==(const ChannelInfo& other);
  };

  class ChannelManager {
   public:
    static ChannelManager& getInstance();

    /**
     * Destructor called on SIGINT!
     * So the map is cleared here.
     */
    ~ChannelManager();

    /**
     * Handler called once the Channel Accesss is closed or opened.
     * It is to be registered with the Channel Access creation.
     */
    static void channelStateHandler(connection_handler_args args);

    /**
     * Handler called once data is updated on a EPICS channel.
     */
    static void handleEvent(evargs args);

    /**
     *  Add channel to the map and open channel access.
     *  \param name The EPICS channel access name.
     *  \param backend The backend pointer to be passed to the CA channel.
     *                 It is used to change the backend state and check if it is still open.
     * \remark map should be locked by calling function!
     */
    void addChannel(const std::string name, EpicsBackend* backend);

    /**
     *  Register all channels in the map and open channel access.
     *
     *  \param backend The backend pointer to be passed to the CA channel.
     *                 It is used to change the backend state and check if it is still open.
     *
     * \remark map should be locked by calling function!
     */

    void addChannelsFromMap(EpicsBackend* backend);

    /**
     * Check if all channels in the map are connected.
     *
     * \return True if all channels are connected.
     */
    bool checkAllConnected();

    /**
     * Check if channel is connected.
     *
     * \param name The EPICS channel access name.
     * \return True if channel is connected.
     */
    bool isChannelConnected(const std::string name);

    /**
     * Remove accessor that is connected to a certain access channel
     * \param name The EPICS channel access name.
     * \param accessor The accessor that is updated by changes from channel access
     */
    void removeAccessor(const std::string& name, EpicsBackendRegisterAccessorBase* accessor);

    /**
     * Push exception to all accessors that are registered
     *
     */
    void setException(const std::string error);

    /**
     * Reset the map content
     */
    void cleanup() { channelMap.clear(); };

    /**
     * Check if channel access meta data is filled.
     * This only happens once the channel access connection is established.
     * See channelStateHandler.
     *
     * \param name The EPICS channel access name.
     * @return True if channel is configured.
     */
    bool isChannelConfigured(const std::string& name);

    /**
     * Get pv pointer.
     *
     * \param name The EPICS channel access name.
     * \return PV pointer
     * \remark map should be locked by calling function!
     */
    std::shared_ptr<pv> getPV(const std::string& name);

    /**
     * Create channel access subscription.
     *
     * \param name The EPICS channel access name.
     */
    void activateChannel(const std::string& name);

    /**
     * Activate all registered channels.
     */
    void activateChannels();

    /**
     * Deactivate all registered channels.
     */
    void deactivateChannels();

    /**
     * Add accessor that is connected to a certain access channel.
     *
     * \param name The EPICS channel access name.
     * \param accessor The accessor that is updated by changes from channel access
     * \remark map should be locked by calling function!
     */
    void addAccessor(const std::string& name, EpicsBackendRegisterAccessorBase* accessor);

    std::mutex mapLock;

   private:
    // map that connects the EPICS PV name to the ChannelInfo object
    std::map<std::string, ChannelInfo> channelMap;

    /**
     *  Check if a channel is registered.
     *  \param name The EPICS channel access name.
     *  \remark map should be locked by calling function!
     */
    bool channelPresent(const std::string name);

    /**
     * Find the entry in the map for a given chanId.
     * \param chid The chanId of the PV.
     * \return Iterator to the internal map.
     * \throw ChimeraTK::runtime_error if chid is not found.
     * \remark Lock the map if you intend to change the map!
     */
    std::map<std::string, ChannelInfo>::iterator findChid(const chanId& chid);
  };
} // namespace ChimeraTK
