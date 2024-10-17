#include <vector>
using namespace std;

// enum for cell_volt_info, cell_temp_info, battery_info, device_info, device_address
enum class BatterySection
{
    CELL_VOLT_INFO = 5000,
    CELL_TEMP_INFO = 5017,
    BATTERY_INFO = 5042,
    DEVICE_INFO = 5122,
    DEVICE_ADDRESS = 5223
};

// enum for battery section words
enum class BatterySectionWords
{
    CELL_VOLT_INFO = 17,
    CELL_TEMP_INFO = 17,
    BATTERY_INFO = 6,
    DEVICE_INFO = 8,
    DEVICE_ADDRESS = 1
};

// enum for modbus function codes
enum class ModbusFunction
{
    READ = 0x03,
    WRITE = 0x06
};

vector<uint8_t> GetBatteryRequest(uint8_t batteryNumber, BatterySection section = BatterySection::BATTERY_INFO, BatterySectionWords words = BatterySectionWords::BATTERY_INFO)
{
    ESP_LOGD("GetBatteryRequest", "Getting battery request for battery %d, section %d, words %d", batteryNumber, static_cast<uint16_t>(section), static_cast<uint16_t>(words));

    // section to uint16_t
    uint16_t sectionInt = static_cast<uint16_t>(section);
    // words to uint16_t
    uint16_t wordsInt = static_cast<uint16_t>(words);

    vector<uint8_t> dataBytes = {
        batteryNumber,
        static_cast<uint8_t>(ModbusFunction::READ),
        static_cast<uint8_t>((sectionInt >> 8) & 0xFF),
        static_cast<uint8_t>((sectionInt >> 0) & 0xFF),
        static_cast<uint8_t>((wordsInt >> 8) & 0xFF),
        static_cast<uint8_t>((wordsInt >> 0) & 0xFF),
    };

    // add checksum to the end
    uint16_t checksum = crc16(&dataBytes[0], dataBytes.size());

    // this needs to be split into 2 bytes
    dataBytes.push_back((checksum >> 0) & 0xFF);
    dataBytes.push_back((checksum >> 8) & 0xFF);

    for (size_t i = 0; i < dataBytes.size(); ++i)
    {
        ESP_LOGV("GetBatteryRequest", "Request Byte %d: 0x%02X", i, dataBytes[i]);
    }
    return dataBytes;
}

void HandleBatteryData(const vector<uint8_t> &x, BatterySection section, string *batteryName = nullptr)
{
    // Log each byte in the array
    for (size_t i = 0; i < x.size(); ++i)
    {
        ESP_LOGV("HandleBatteryData", "Response Byte %d: 0x%02X", i, x[i]);
    }

    // // if something went wrong, we didn't get the data we expected, so bail out here
    // if (receivedSize < 17)
    //     return NAN;

    ESP_LOGD("HandleBatteryData", "Expecting section: %d", static_cast<uint16_t>(section));

    uint8_t batteryId;
    std::memcpy(&batteryId, &x[0], sizeof(batteryId));
    ESP_LOGD("HandleBatteryData", "battery Id: %d", batteryId);

    // Parse the function (3 == Read)
    uint8_t function;
    std::memcpy(&function, &x[1], sizeof(function));
    // function = ntohs(function); // Convert from network byte order to host byte order
    ESP_LOGD("HandleBatteryData", "Function: %d", function);

    // Check and exit if function is not 3
    if (function != static_cast<uint8_t>(ModbusFunction::READ))
    {
        ESP_LOGE("HandleBatteryData", "Unexpected function: %d", function);
        return;
    }

    // Check if batteryName is null
    if (batteryName == nullptr)
    {
        ESP_LOGD("HandleBatteryData", "Battery name is null");
        // initialize batteryName pointer
        batteryName = new string();
        // set batteryName to a value of batteryNumber
        *batteryName = to_string(batteryId) + " ";
    }
    else
    {
        // if batteryName is not null but empty
        if (!batteryName->empty())
        {
            // add space after batteryName
            batteryName->append(" ");
        }
    }

    ESP_LOGD("HandleBatteryData", "Battery sensor prefix: %s", batteryName->c_str());

    // Switch on the section
    switch (section)
    {
    case BatterySection::BATTERY_INFO: // Battery Info
    {
        ESP_LOGD("HandleBatteryData", "Parsing Battery Info");

        // Parse the current
        int16_t current;
        std::memcpy(&current, &x[3], sizeof(current));
        current = ntohs(current); // Convert from network byte order to host byte order
        ESP_LOGD("HandleBatteryData", "current: %d", current);

        // Parse the voltage
        uint16_t voltage;
        std::memcpy(&voltage, &x[5], 2);
        voltage = ntohs(voltage); // Convert from network byte order to host byte order
        ESP_LOGD("HandleBatteryData", "voltage: %d", voltage);

        // Parse the present capacity
        uint32_t presentCapacity;
        std::memcpy(&presentCapacity, &x[7], 4);
        presentCapacity = ntohl(presentCapacity); // Convert from network byte order to host byte order
        ESP_LOGD("HandleBatteryData", "presentCapacity: %d", presentCapacity);

        // Parse the total capacity
        uint32_t totalCapacity;
        std::memcpy(&totalCapacity, &x[11], 4);
        totalCapacity = ntohl(totalCapacity); // Convert from network byte order to host byte order
        ESP_LOGD("HandleBatteryData", "totalCapacity: %d", totalCapacity);

        // Convert the values to the appropriate units
        float currentFloat = static_cast<float>(current) / 100.0f;
        float voltageFloat = static_cast<float>(voltage) / 10.0f;
        float presentCapacityFloat = static_cast<float>(presentCapacity) / 1000.0f;
        float totalCapacityFloat = static_cast<float>(totalCapacity) / 1000.0f;
        float chargeLevelFloat = (presentCapacityFloat / totalCapacityFloat) * 100.0f;

        ESP_LOGD("HandleBatteryData", "currentFloat: %.1f", currentFloat);
        ESP_LOGD("HandleBatteryData", "voltageFloat: %.1f", voltageFloat);
        ESP_LOGD("HandleBatteryData", "presentCapacityFloat: %.1f", presentCapacityFloat);
        ESP_LOGD("HandleBatteryData", "totalCapacityFloat: %.1f", totalCapacityFloat);
        ESP_LOGD("HandleBatteryData", "chargeLevelFloat: %.1f", chargeLevelFloat);

        // Publish the values
        string current_id = batteryName->c_str();
        current_id.append("Current");

        string voltage_id = batteryName->c_str();
        voltage_id.append("Voltage");

        string present_capacity_id = batteryName->c_str();
        present_capacity_id.append("Present Capacity");

        string total_capacity_id = batteryName->c_str();
        total_capacity_id.append("Total Capacity");

        string charge_level_id = batteryName->c_str();
        charge_level_id.append("Charge Level");

        for (auto *obj : App.get_sensors())
        {
            ESP_LOGV("HandleBatteryData", "Looping sensors. Current name: %s", obj->get_name());
            if (obj->get_name().str().compare(current_id) == 0)
            {
                obj->publish_state(currentFloat);
            }
            else if (obj->get_name().str().compare(voltage_id) == 0)
            {
                obj->publish_state(voltageFloat);
            }
            else if (obj->get_name().str().compare(present_capacity_id) == 0)
            {
                obj->publish_state(presentCapacityFloat);
            }
            else if (obj->get_name().str().compare(total_capacity_id) == 0)
            {
                obj->publish_state(totalCapacityFloat);
            }
            else if (obj->get_name().str().compare(charge_level_id) == 0)
            {
                obj->publish_state(chargeLevelFloat);
            }
        }

        break;
    }
    case BatterySection::CELL_TEMP_INFO: // Cell Temp Info
    {
        ESP_LOGD("HandleBatteryData", "Parsing Cell Temp Info");

        // Get sensor count
        uint16_t sensorCount;
        std::memcpy(&sensorCount, &x[3], sizeof(sensorCount));
        sensorCount = ntohs(sensorCount); // Convert from network byte order to host byte order
        ESP_LOGD("HandleBatteryData", "sensorCount: %d", sensorCount);

        // Parse the sensor temps
        int16_t sensorTemp[sensorCount];
        for (int i = 0; i < sensorCount; i++)
        {
            std::memcpy(&sensorTemp[i], &x[5 + (i * 2)], sizeof(sensorTemp[i]));
            sensorTemp[i] = ntohs(sensorTemp[i]); // Convert from network byte order to host byte order
            ESP_LOGD("HandleBatteryData", "sensorTemp[%d]: %d", i, sensorTemp[i]);
        }

        // Convert the values to the appropriate units
        float sensorTempFloat[sensorCount];
        for (int i = 0; i < sensorCount; i++)
        {
            sensorTempFloat[i] = static_cast<float>(sensorTemp[i]) / 10.0f;
            ESP_LOGD("HandleBatteryData", "sensorTempFloat[%d]: %.1f", i, sensorTempFloat[i]);
        }

        // Publish the sensor temps
        for (int i = 0; i < sensorCount; i++)
        {
            string sensor_temp_id = batteryName->c_str();
            sensor_temp_id.append("Sensor ");
            sensor_temp_id.append(to_string(i + 1));
            sensor_temp_id.append(" Temperature");

            for (auto *obj : App.get_sensors())
            {
                ESP_LOGV("HandleBatteryData", "Looping sensors. Current name: %s", obj->get_name());
                if (obj->get_name().str().compare(sensor_temp_id) == 0)
                {
                    obj->publish_state(sensorTempFloat[i]);
                }
            }
        }

        break;
    }
    case BatterySection::CELL_VOLT_INFO: // Cell Volt Info
    {
        ESP_LOGD("HandleBatteryData", "Parsing Cell Volt Info");

        // Get cell count
        uint16_t cellCount;
        std::memcpy(&cellCount, &x[3], sizeof(cellCount));
        cellCount = ntohs(cellCount); // Convert from network byte order to host byte order
        ESP_LOGD("HandleBatteryData", "cellCount: %d", cellCount);

        // Parse the cell volts
        uint16_t cellVolt[cellCount];
        for (int i = 0; i < cellCount; i++)
        {
            std::memcpy(&cellVolt[i], &x[5 + (i * 2)], sizeof(cellVolt[i]));
            cellVolt[i] = ntohs(cellVolt[i]); // Convert from network byte order to host byte order
            ESP_LOGD("HandleBatteryData", "cellVolt[%d]: %d", i, cellVolt[i]);
        }

        // Convert the values to the appropriate units
        float cellVoltFloat[cellCount];
        for (int i = 0; i < cellCount; i++)
        {
            cellVoltFloat[i] = static_cast<float>(cellVolt[i]) / 10.0f;
            ESP_LOGD("HandleBatteryData", "cellVoltFloat[%d]: %.3f", i, cellVoltFloat[i]);
        }

        // Publish the cell volts
        for (int i = 0; i < cellCount; i++)
        {
            string cell_volt_id = batteryName->c_str();
            cell_volt_id.append("Cell ");
            cell_volt_id.append(to_string(i + 1));
            cell_volt_id.append(" Voltage");

            for (auto *obj : App.get_sensors())
            {
                ESP_LOGV("HandleBatteryData", "Looping sensors. Current name: %s", obj->get_name());
                if (obj->get_name().str().compare(cell_volt_id) == 0)
                {
                    obj->publish_state(cellVoltFloat[i]);
                }
            }
        }
        break;
    }
    case BatterySection::DEVICE_INFO: // Device Info
    {
        ESP_LOGD("HandleBatteryData", "Parsing Device Info");

        // Parse the device model as string from bytes 3-17
        char device_model[15];
        std::memcpy(&device_model, &x[3], sizeof(device_model));
        ESP_LOGD("HandleBatteryData", "device_model: %s", device_model);

        break;
    }
    case BatterySection::DEVICE_ADDRESS: // Device Address
    {
        ESP_LOGD("HandleBatteryData", "Parsing Device Address");

        // Parse deviceId
        uint16_t device_id;
        std::memcpy(&device_id, &x[3], sizeof(device_id));
        device_id = ntohs(device_id); // Convert from network byte order to host byte order
        ESP_LOGD("HandleBatteryData", "device_id: %d", device_id);

        break;
    }
    default:
    {
        ESP_LOGE("HandleBatteryData", "Unknown section: %d", section);
        break;
    }
    }
}