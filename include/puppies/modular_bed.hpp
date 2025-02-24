#pragma once

#include "puppies/PuppyModbus.hpp"
#include "module/modular_heatbed.h"
#include <modular_bed_errors.hpp>
#include <modular_bed_registers.hpp>
#include <utility_extensions.hpp>

namespace buddy::puppies {

constexpr size_t bedlet_idx_to_board_number(size_t idx) {
    return idx + 1;
}

class ModularBed : public ModbusDevice, public SimpleModularHeatbed, public AdvancedModularBed {
public:
    static constexpr auto BEDLET_PERIOD_MS = 300;
    static constexpr auto BEDLET_MAX_X = 4;
    static constexpr auto BEDLET_MAX_Y = 4;
    static constexpr auto BEDLET_COUNT = BEDLET_MAX_X * BEDLET_MAX_Y;

    using SystemDiscreteInput = modular_bed_shared::registers::SystemDiscreteInput;
    using HBDiscreteInput = modular_bed_shared::registers::HBDiscreteInput;
    using SystemInputRegister = modular_bed_shared::registers::SystemInputRegister;
    using SystemCoil = modular_bed_shared::registers::SystemCoil;
    using HBInputRegister = modular_bed_shared::registers::HBInputRegister;
    using HBHoldingRegister = modular_bed_shared::registers::HBHoldingRegister;
    using HBCoil = modular_bed_shared::registers::HBCoil;

    static constexpr uint16_t GENERAL_DISCRETE_INPUTS_ADDR { ftrstd::to_underlying(SystemDiscreteInput::power_painc_status) };
    static constexpr uint16_t BEDLET_DISCRETE_INPUTS_ADDR { ftrstd::to_underlying(HBDiscreteInput::is_ready) };
    static constexpr uint16_t FAULT_STATUS_ADDR { ftrstd::to_underlying(SystemInputRegister::fault_status) };
    static constexpr uint16_t STATIC_INPUT_REGISTERS_ADDR { ftrstd::to_underlying(SystemInputRegister::hw_bom_id) };
    static constexpr uint16_t CURRENTS_ADDR { ftrstd::to_underlying(SystemInputRegister::adc_current_1) };
    static constexpr uint16_t CLEAR_FAULT_ADDR { ftrstd::to_underlying(SystemCoil::clear_fault_status) };
    static constexpr uint16_t RESET_OVECURRENT_FAULT_ADDR { ftrstd::to_underlying(SystemCoil::reset_overcurrent_fault) };
    static constexpr uint16_t TEST_HEATING_ADDR { ftrstd::to_underlying(SystemCoil::test_hb_heating) };
    static constexpr uint16_t PRINT_FAN_ACTIVE_ADDR { ftrstd::to_underlying(SystemCoil::print_fan_active) };
    static constexpr uint16_t BEDLET_INPUT_REGISTERS_ADDR { ftrstd::to_underlying(HBInputRegister::fault_status) };
    static constexpr uint16_t BEDLET_TARGET_TEMP_ADDR { ftrstd::to_underlying(HBHoldingRegister::target_temperature) };
    static constexpr uint16_t BEDLET_MEASURED_MAX_CURRENT_ADDR { ftrstd::to_underlying(HBHoldingRegister::measured_max_current) };
    static constexpr uint16_t BEDLET_CLEAR_FAULT_ADDR { ftrstd::to_underlying(HBCoil::clear_fault_status) };
    static constexpr uint16_t MCU_TEMPERATURE_ADDR { ftrstd::to_underlying(SystemInputRegister::mcu_temperature) };

    enum class CommunicationStatus {
        OK,
        ERROR,
    };

    using SystemError = modular_bed_shared::errors::SystemError;
    using HeatbedletError = modular_bed_shared::errors::HeatbedletError;

    ModularBed(PuppyModbus &bus, uint8_t modbus_address);
    ModularBed(const ModularBed &) = delete;

    CommunicationStatus ping();
    CommunicationStatus initial_scan();
    CommunicationStatus refresh();

    void clear_fault();

    // Simple not bedlet aware API
    void set_target(float target_temp) override;
    float get_temp() override;

    // Smart bed API (allows separate control of bedlets)
    void set_target(const uint8_t column, const uint8_t row, float target_temp) override;
    void set_target(const uint8_t idx, float target_temp);
    float get_temp(const uint16_t idx);
    float get_temp(const uint8_t column, const uint8_t row) override;

    // Convert x,y to index
    uint16_t idx(const uint8_t column, const uint8_t row);

    // Obsolete, marlin does not need to know internal bedlet PWMs
    int get_pwm() override; // TODO: Remove

    // Set non-enabled bedlet temperatures so warping of the bed is avoided
    void update_bedlet_temps(uint16_t enabled_mask, float target_temp);

    // notify modular bed about activity of print fan (to relax temperature checks)
    void set_print_fan_active(bool active);

    MODBUS_DISCRETE GeneralStatus {
        bool power_panic_status;
        bool current_fault_status;
    };

    MODBUS_REGISTER GeneralStatic {
        uint16_t hw_bom_id;
        uint32_t hw_otp_timestamp;
        uint16_t hw_data_matrix[12];
        uint16_t heatbedlet_count;
    };

    MODBUS_REGISTER BedletData {
        HeatbedletError fault_status[BEDLET_COUNT];
        uint16_t measured_temperature[BEDLET_COUNT];
        uint16_t pwm_state[BEDLET_COUNT];
        uint16_t p_value[BEDLET_COUNT];
        uint16_t i_value[BEDLET_COUNT];
        uint16_t d_value[BEDLET_COUNT];
        uint16_t tc_value[BEDLET_COUNT];
    };

    ModbusDiscreteInputBlock<GENERAL_DISCRETE_INPUTS_ADDR, GeneralStatus> general_status;
    ModbusInputRegisterBlock<STATIC_INPUT_REGISTERS_ADDR, GeneralStatic> general_static;
    ModbusInputRegisterBlock<BEDLET_INPUT_REGISTERS_ADDR, BedletData> bedlet_data;
    ModbusHoldingRegisterBlock<BEDLET_TARGET_TEMP_ADDR, uint16_t[BEDLET_COUNT]> bedlet_target_temp;
    ModbusHoldingRegisterBlock<BEDLET_MEASURED_MAX_CURRENT_ADDR, uint16_t[BEDLET_COUNT]> bedlet_measured_max_current;
    ModbusInputRegisterBlock<CURRENTS_ADDR, int16_t[2]> currents;
    ModbusDiscreteInputBlock<BEDLET_DISCRETE_INPUTS_ADDR, bool> general_ready;
    ModbusInputRegisterBlock<FAULT_STATUS_ADDR, SystemError> general_fault;
    ModbusInputRegisterBlock<MCU_TEMPERATURE_ADDR, uint16_t> mcu_temperature;
    ModbusCoil<CLEAR_FAULT_ADDR> clear_fault_status;
    ModbusCoil<RESET_OVECURRENT_FAULT_ADDR> reset_overcurrent;
    ModbusCoil<TEST_HEATING_ADDR> test_heating;
    ModbusCoil<PRINT_FAN_ACTIVE_ADDR> print_fan_active;

protected:
    struct cost_and_enable_mask_t {
        uint8_t cost;
        uint16_t enable_mask;
    };

    struct side_t {
        uint8_t dim;
        int8_t sign;
    };

    // update gradients of enabled bedlets so that temperature is not too different between neighbouring bedlets
    void update_gradients(uint16_t enabled_mask);
    // calculate cost and bedlets to enable, when we want to to enable bedlets towards one specific side
    cost_and_enable_mask_t touch_side(uint16_t enabled_mask, side_t side);

    // calculate what betlets to enable so that betlets towards two sides are heated
    uint16_t expand_to_sides(uint16_t enabled_mask, float target_temp);
};

extern ModularBed modular_bed;

}
