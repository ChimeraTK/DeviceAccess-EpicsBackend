// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * EPICSSubscriptionManager.h
 *
 *  Created on: Mar 23, 2021
 *      Author: Klaus Zenker (HZDR)
 */
#include "EPICS_types.h"

#include <deque>
#include <map>
#include <mutex>
#include <string>

namespace ChimeraTK {
  class EpicsBackendRegisterAccessorBase;

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

    struct ChannelInfo {
      std::deque<EpicsBackendRegisterAccessorBase*> _accessors;
      std::string _caName;
      bool _connected{false};
      ChannelInfo(std::string channelName) : _caName(channelName){};

      bool isChannelName(std::string channelName);

      bool operator==(const ChannelInfo& other);
    };

    // Register a channel.
    void addChannel(const chid& chidIn, const std::string name);

    /*
     *  Check if a channel is registered.
     *  \param name The name of the registered channel access
     */
    bool channelPresent(const std::string name);

    /**
     * Add accessor that is connected to a certain access channel
     * \param chid Channel ID
     * \param accessor The accessor that is updated by changes from channel access
     */
    void addAccessor(const chid& chid, EpicsBackendRegisterAccessorBase* accessor);

    /**
     * Remove accessor that is connected to a certain access channel
     * \param chid Channel ID
     * \param accessor The accessor that is updated by changes from channel access
     */
    void removeAccessor(const chid& chidIn, EpicsBackendRegisterAccessorBase* accessor);

    /**
     * Push exception to all accessors that are registered
     *
     */
    void setException(const std::string error);

    /**
     * Reset the map content and disconnect channels.
     */
    void cleanup();

    /**
     * Wait until all channels are connected.
     *
     * \param timeout: Timeout in seconds used to wait for connections.
     *
     * \throw ChimeraTK::runtime_error if not all channels are connected within the timeout.
     */
    void waitForConnections(double timeout = 1.0);

   private:
    std::mutex mapLock;

    std::map<chid, ChannelInfo> channelMap;

    /**
     * Check if all Channel Access channels are connected.
     */
    bool checkChannels();
  };
} // namespace ChimeraTK
