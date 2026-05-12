# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/siovush/esp/esp-idf/components/bootloader/subproject"
  "/home/siovush/esp/esp32_s3_web_plc/build/bootloader"
  "/home/siovush/esp/esp32_s3_web_plc/build/bootloader-prefix"
  "/home/siovush/esp/esp32_s3_web_plc/build/bootloader-prefix/tmp"
  "/home/siovush/esp/esp32_s3_web_plc/build/bootloader-prefix/src/bootloader-stamp"
  "/home/siovush/esp/esp32_s3_web_plc/build/bootloader-prefix/src"
  "/home/siovush/esp/esp32_s3_web_plc/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/siovush/esp/esp32_s3_web_plc/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/siovush/esp/esp32_s3_web_plc/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
