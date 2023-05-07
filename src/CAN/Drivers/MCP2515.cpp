#include "MCP2515.hpp"

using namespace MCP2515;

DriverMCP2515* self;

bool DriverMCP2515::setMode(CanDriverMode mode)
{
  bool success = false;

  switch (mode)
  {
  case CanDriverMode::Normal:
    success = mcp2515->setNormalMode();
    break;

  case CanDriverMode::Loopback:
    success = mcp2515->setLoopbackMode();
    break;

  case CanDriverMode::ListenOnly:
    success = mcp2515->setListenOnlyMode();
    break;
  }

  if(success)
    currentMode = mode;

  return success;
}

void DriverMCP2515::writeLoopbackTest()
{
  debugSerial.println("CAN running loopback test");

  CanDriverMode modeBeforeTest = currentMode;
  setMode(CanDriverMode::Loopback);

  CanFrame const test_frame_1 = { 0x00000001, {0}, 0, false, false};                                              /* Minimum (no) payload */
  CanFrame const test_frame_2 = { 0x00000002, {0xCA, 0xFE, 0xCA, 0xFE, 0, 0, 0, 0}, 4, false, false };             /* Between minimum and maximum payload */
  CanFrame const test_frame_3 = { 0x00000003, {0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE, 0xCA, 0xFE}, 8, false, false }; /* Maximum payload */
  CanFrame const test_frame_4 = { 0x40000004, {0}, 0, false, false };                                              /* RTR frame */
  CanFrame const test_frame_5 = { 0x000007FF, {0}, 0, false, false };                                              /* Highest standard 11 bit CAN address */
  CanFrame const test_frame_6 = { 0x80000000, {0}, 0, false, false };                                              /* Lowest extended 29 bit CAN address */
  CanFrame const test_frame_7 = { 0x9FFFFFFF, {0}, 0, false, false };                                              /* Highest extended 29 bit CAN address */

  std::array<CanFrame, 7> const CAN_TEST_FRAME_ARRAY =
  {
    test_frame_1,
    test_frame_2,
    test_frame_3,
    test_frame_4,
    test_frame_5,
    test_frame_6,
    test_frame_7
  };

  std::for_each(CAN_TEST_FRAME_ARRAY.cbegin(),
                CAN_TEST_FRAME_ARRAY.cend(),
                [this](CanFrame const frame)
                {
                if(!mcp2515->transmit(frame.id, frame.data, frame.len)) {
                    debugSerial.println("ERROR TX");
                }
                delay(10);
                });

  setMode(modeBeforeTest);
}

void DriverMCP2515::handleInterrupt()
{
  if(!canInited)
    return;

  mcp2515->onExternalEventHandler();
}

bool DriverMCP2515::getRate(const uint8_t mhz, const uint16_t speed, CanBitRate &rate)
{
  bool found = true;

  if(mhz == 8)
  {
    switch (CANConfig->CAN_SPEED_KBPS)
    {
    case 10:
      rate = CanBitRate::BR_10kBPS_8MHZ;
      break;

    case 20:
      rate = CanBitRate::BR_20kBPS_8MHZ;
      break;

    case 50:
      rate = CanBitRate::BR_50kBPS_8MHZ;
      break;

    case 100:
      rate = CanBitRate::BR_100kBPS_8MHZ;
      break;

    case 125:
      rate = CanBitRate::BR_125kBPS_8MHZ;
      break;

    case 250:
      rate = CanBitRate::BR_250kBPS_8MHZ;
      break;

    case 500:
      rate = CanBitRate::BR_500kBPS_8MHZ;
      break;

    case 800:
      rate = CanBitRate::BR_800kBPS_8MHZ;
      break;

    case 1000:
      rate = CanBitRate::BR_1000kBPS_8MHZ;
      break;

    default:
      found = false;
      break;
    }
  }
  else if(mhz == 10)
  {
    switch (CANConfig->CAN_SPEED_KBPS)
    {
    case 10:
      rate = CanBitRate::BR_10kBPS_10MHZ;
      break;

    case 20:
      rate = CanBitRate::BR_20kBPS_10MHZ;
      break;

    case 50:
      rate = CanBitRate::BR_50kBPS_10MHZ;
      break;

    case 100:
      rate = CanBitRate::BR_100kBPS_10MHZ;
      break;

    case 125:
      rate = CanBitRate::BR_125kBPS_10MHZ;
      break;

    case 250:
      rate = CanBitRate::BR_250kBPS_10MHZ;
      break;

    case 500:
      rate = CanBitRate::BR_500kBPS_10MHZ;
      break;

    case 1000:
      rate = CanBitRate::BR_1000kBPS_10MHZ;
      break;

    default:
      found = false;
      break;
    }
  }
  else if(mhz == 12)
  {
    switch (CANConfig->CAN_SPEED_KBPS)
    {
    case 10:
      rate = CanBitRate::BR_10kBPS_12MHZ;
      break;

    case 20:
      rate = CanBitRate::BR_20kBPS_12MHZ;
      break;

    case 50:
      rate = CanBitRate::BR_50kBPS_12MHZ;
      break;

    case 100:
      rate = CanBitRate::BR_100kBPS_12MHZ;
      break;

    case 125:
      rate = CanBitRate::BR_125kBPS_12MHZ;
      break;

    case 250:
      rate = CanBitRate::BR_250kBPS_12MHZ;
      break;

    case 500:
      rate = CanBitRate::BR_500kBPS_12MHZ;
      break;

    case 1000:
      rate = CanBitRate::BR_1000kBPS_12MHZ;
      break;

    default:
      found = false;
      break;
    }
  }
  else if(mhz == 16)
  {
    switch (CANConfig->CAN_SPEED_KBPS)
    {
    case 10:
      rate = CanBitRate::BR_10kBPS_16MHZ;
      break;

    case 20:
      rate = CanBitRate::BR_20kBPS_16MHZ;
      break;

    case 50:
      rate = CanBitRate::BR_50kBPS_16MHZ;
      break;

    case 100:
      rate = CanBitRate::BR_100kBPS_16MHZ;
      break;

    case 125:
      rate = CanBitRate::BR_125kBPS_16MHZ;
      break;

    case 250:
      rate = CanBitRate::BR_250kBPS_16MHZ;
      break;

    case 500:
      rate = CanBitRate::BR_500kBPS_16MHZ;
      break;

    case 800:
      rate = CanBitRate::BR_800kBPS_16MHZ;
      break;

    case 1000:
      rate = CanBitRate::BR_1000kBPS_16MHZ;
      break;

    default:
      found = false;
      break;
    }
  }
  else
  {
    found = false;
  }

  return found;
}

DriverMCP2515::DriverMCP2515(const CAN_Config* CANConfig) : CANDriver(CANConfig)
{
  self = this;

  mcp2515 = new ArduinoMCP2515([this]()
                              {
                                SPI.beginTransaction(MCP2515x_SPI_SETTING);
                                digitalWrite(this->CANConfig->CAN_SPI.PIN_CS, LOW);
                              },
                              [this]()
                              {
                                digitalWrite(this->CANConfig->CAN_SPI.PIN_CS, HIGH);
                                SPI.endTransaction();
                              },
                              [this](uint8_t const dataByte) { return SPI.transfer(dataByte); },
                              micros,
                              [this](const uint32_t timestamp_us, const uint32_t id, const uint8_t *data, const uint8_t len)
                              {
                                CanFrame frame;
                                frame.id = id;
                                memcpy(frame.data, data, len);
                                frame.len = len;
                                frame.isEXT = id & MCP2515::CAN_EFF_BITMASK;
                                frame.isRTR = id & MCP2515::CAN_RTR_BITMASK;

                                onDataRecieved(timestamp_us, frame);
                              },
                              nullptr);
}

bool DriverMCP2515::initInterface()
{
  CanBitRate rate;

  bool ratePossible = getRate(CANConfig->CAN_SPI.IC_MHZ, CANConfig->CAN_SPEED_KBPS, rate);

  if(!ratePossible) // test if we can write something to the MCP2515 (is a device connected?)
  {
      debugSerial.println("CAN-Bus init failed! E1");
      return false;
  }

  /* Setup SPI access */
  SPI.begin(CANConfig->CAN_SPI.PIN_SCK,
            CANConfig->CAN_SPI.PIN_MISO,
            CANConfig->CAN_SPI.PIN_MOSI,
            CANConfig->CAN_SPI.PIN_CS);

  pinMode(CANConfig->CAN_SPI.PIN_CS, OUTPUT);
  digitalWrite(CANConfig->CAN_SPI.PIN_CS, HIGH);

  /* Attach interrupt handler to register MCP2515 signaled by taking INT low */
  pinMode(CANConfig->CAN_SPI.PIN_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CANConfig->CAN_SPI.PIN_INT),
                  []() IRAM_ATTR
                  {
                    self->handleInterrupt();
                  },
                  FALLING);

  mcp2515->begin();

  if(!setMode(CanDriverMode::Loopback)) // test if we can write something to the MCP2515 (is a device connected?)
  {
      SPI.end();
      debugSerial.println("CAN-Bus init failed! E2");
      return false;
  }

  mcp2515->setBitRate(rate); // CAN bit rate and MCP2515 clock speed

  defaultInit();

  return true;
}

void DriverMCP2515::sendCommand(CANCommand* cmd, bool setValue, int value)
{
  if(currentMode == CanDriverMode::ListenOnly)
    return;

  CanFrame* frame = getCanFrameFromCommand(cmd, setValue, value);

  if(!mcp2515->transmit(frame->id, frame->data, frame->len)) {
    debugSerial.println("ERROR TX");
  }

  delete frame;
}

DriverMCP2515::~DriverMCP2515()
{
  self = nullptr;
  delete mcp2515;
}