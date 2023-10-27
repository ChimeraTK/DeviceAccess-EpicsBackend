// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * testPauseIOC.C
 *
 *  Created on: Oct 26, 2023
 *      Author: Klaus Zenker (HZDR)
 */

#include "DummyIOC.h"

class IOCLauncher {
 public:
  IOCLauncher() {
    helper = &_helper;
    _helper.start();
  }
  IOCHelper _helper;
  static IOCHelper* helper;
};

IOCHelper* IOCLauncher::helper;

int main() {
  IOCLauncher l;
  IOCLauncher::helper->pause(true);
  std::cout << "Pause active." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(5));
  IOCLauncher::helper->pause(false);
  std::cout << "IOC active." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(5));
  IOCLauncher::helper->stop();
  return 1;
}
