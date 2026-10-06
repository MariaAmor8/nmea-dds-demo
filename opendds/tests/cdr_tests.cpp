#include "dds.hpp"
#include <dds/DCPS/Serializer.h>
#include <ace/Message_Block.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition) { if (!condition) throw std::runtime_error("CDR roundtrip mismatch"); }
}
int main(int, char*[]) {
  try {
    for (const auto endian : {OpenDDS::DCPS::ENDIAN_LITTLE, OpenDDS::DCPS::ENDIAN_BIG}) {
      for (bool simulated : {false, true}) {
        for (const auto& raw : {std::string(), std::string("GPS1 on UDP2: $GPRMC,131537.83,A,0000.95081,S,00000.29512,W,0010.0,089.0,051026,0.0,W,A,S*66"), std::string(8192, 'x')}) {
          auto original = marine::synthetic(0xffffffffU);
          original.raw_nmea = raw;
          original.timestamp_ms = 0xfedcba9876543210ULL;
          original.simulated = simulated;
          original.position_valid = simulated;
          original.heading_valid = !simulated;
          original.depth_valid = simulated;
          const auto wire = marine::to_dds(original);
          ACE_Message_Block block(16384);
          const OpenDDS::DCPS::Encoding encoding(OpenDDS::DCPS::Encoding::KIND_XCDR1, endian);
          OpenDDS::DCPS::Serializer writer(&block, encoding);
          require(writer << wire);
          Marine::Navigation restored;
          OpenDDS::DCPS::Serializer reader(&block, encoding);
          require(reader >> restored);
          const auto result = marine::from_dds(restored);
          require(result.raw_nmea == original.raw_nmea && result.sequence == original.sequence &&
                  result.timestamp_ms == original.timestamp_ms && result.latitude_deg == original.latitude_deg &&
                  result.longitude_deg == original.longitude_deg && result.speed_knots == original.speed_knots &&
                  result.course_deg == original.course_deg && result.heading_deg == original.heading_deg &&
                  result.depth_m == original.depth_m && result.position_valid == original.position_valid &&
                  result.heading_valid == original.heading_valid && result.depth_valid == original.depth_valid &&
                  result.simulated == original.simulated);
          require(block.length() == 0);
        }
      }
    }
    std::cout << "All Navigation fields passed CDR roundtrip (both byte orders)\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
