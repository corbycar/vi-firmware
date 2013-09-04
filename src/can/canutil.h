#ifndef _CANUTIL_H_
#define _CANUTIL_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "util/bitfield.h"
#include "util/timer.h"
#include "util/statistics.h"
#include "emqueue.h"
#include "cJSON.h"

#ifdef __LPC17XX__
#include "platform/lpc17xx/canutil_lpc17xx.h"
#endif // __LPC17XX__

#define BUS_MEMORY_BUFFER_SIZE 2 * 8 * 16

// TODO These structs are defined outside of the openxc::can namespace because
// we're not able to used namespaced types with emqueue.

/* Public: A state encoded (SED) signal's mapping from numerical values to
 * OpenXC state names.
 *
 * value - The integer value of the state on the CAN bus.
 * name  - The corresponding string name for the state in OpenXC.
 */
struct CanSignalState {
    int value;
    const char* name;
};
typedef struct CanSignalState CanSignalState;

/* Public: A CAN signal to decode from the bus and output over USB.
 *
 * message     - The message this signal is a part of.
 * genericName - The name of the signal to be output over USB.
 * bitPosition - The starting bit of the signal in its CAN message (assuming
 *               non-inverted bit numbering, i.e. the most significant bit of
 *               each byte is 0)
 * bitSize     - The width of the bit field in the CAN message.
 * factor      - The final value will be multiplied by this factor. Use 1 if you
 *               don't need a factor.
 * offset      - The final value will be added to this offset. Use 0 if you
 *               don't need an offset.
 * minValue    - The minimum value for the processed signal.
 * maxValue    - The maximum value for the processed signal.
 * frequencyClock - A FrequencyClock struct to control the maximum frequency to
 *              process and send this signal. To process every value, set the
 *              clock's frequency to 0.
 * the signal
 * The maximum frequency of this signal to process and send (Hz).
 *              To process every value, set this to 0 (the default).
 * sendSame    - If true, will re-send even if the value hasn't changed.
 * forceSendChanged - If true, regardless of the frequency, it will send the
 *              value if it has changed.
 * states      - An array of CanSignalState describing the mapping
 *               between numerical and string values for valid states.
 * stateCount  - The length of the states array.
 * writable    - True if the signal is allowed to be written from the USB host
 *               back to CAN. Defaults to false.
 * writeHandler - An optional function to encode a signal value to be written to
 *                CAN into a uint64_t. If null, the default encoder is used.
 * received    - Marked true if this signal has ever been received.
 * lastValue   - The last received value of the signal. Defaults to undefined.
 */
struct CanSignal {
    struct CanMessage* message;
    const char* genericName;
    int bitPosition;
    int bitSize;
    float factor;
    float offset;
    float minValue;
    float maxValue;
    openxc::util::time::FrequencyClock frequencyClock;
    bool sendSame;
    bool forceSendChanged;
    CanSignalState* states;
    int stateCount;
    bool writable;
    uint64_t (*writeHandler)(struct CanSignal*, struct CanSignal*, int, cJSON*, bool*);
    bool received;
    float lastValue;
};
typedef struct CanSignal CanSignal;

/* Public: A CAN message, particularly for writing to CAN.
 *
 * bus - A pointer to the bus this message is on.
 * id - The ID of the message.
 * data  - The message's data field.
 */
struct CanMessage {
    struct CanBus* bus;
    uint32_t id;
    uint64_t data;
};
typedef struct CanMessage CanMessage;

QUEUE_DECLARE(CanMessage, 64);

/* Public: A container for a CAN module paried with a certain bus.
 *
 * speed - The bus speed in bits per second (e.g. 500000)
 * address - The address or ID of this node
 * controller - a reference to the CAN controller in the MCU
 *      (platform dependent, needs to be casted to actual type before use).
 * interruptHandler - a function to call by the Interrupt Service Routine when
 *      a previously registered CAN event occurs. (Only used by chipKIT, which
 *      registers a different handler per channel. LPC17xx uses the same global
 *      CAN_IRQHandler.
 * writeHandler - a function that actually writes out a CanMessage object to the
 *      CAN interface (implementation is platform specific);
 * lastMessageReceived - the time (in ms) when the last CAN message was
 *      received. If no message has been received, it should be 0.
 * buffer - message area for 2 channels to store 8 16 byte messages.
 * sendQueue - a queue of CanMessage instances that need to be written to CAN.
 * receiveQueue - a queue of messages received from CAN that have yet to be
 *      translated.
 */
struct CanBus {
    unsigned int speed;
    int address;
    void* controller;
    void (*interruptHandler)();
    bool (*writeHandler)(CanBus*, CanMessage);
    unsigned long lastMessageReceived;
    unsigned int messagesReceived;
    unsigned int messagesDropped;
    openxc::util::statistics::DeltaStatistic totalMessageStats;
    openxc::util::statistics::DeltaStatistic droppedMessageStats;
    openxc::util::statistics::DeltaStatistic receivedMessageStats;
    openxc::util::statistics::DeltaStatistic receivedDataStats;
    uint8_t buffer[BUS_MEMORY_BUFFER_SIZE];
    QUEUE_TYPE(CanMessage) sendQueue;
    QUEUE_TYPE(CanMessage) receiveQueue;
};
typedef struct CanBus CanBus;

/** Public: A parent wrapper for a particular set of CAN messages and associated
 *  CAN buses(e.g. a vehicle or program).
 *
 *  index - A numerical ID for the message set, ideally the index in an array
 *      for fast lookup
 *  name - The name of the message set.
 *  busCount - The number of CAN buses defined for this message set.
 *  messageCount - The number of CAN messages (across all buses) defined for
 *      this message set.
 *  signalCount - The number of CAN signals (across all messages) defined for
 *      this message set.
 *  commandCount - The number of CanCommmands defined for this message set.
 */
typedef struct {
    int index;
    const char* name;
    int busCount;
    int messageCount;
    int signalCount;
    int commandCount;
} CanMessageSet;

namespace openxc {
namespace can {

extern const int CAN_ACTIVE_TIMEOUT_S;

/* Public: A CAN transceiver message filter.
 *
 * number - The ID of this filter, e.g. 0, 1, 2.
 * value - The filter's value.
 * channel - The CAN channel this filter should be applied to - on the PIC32,
 *           channel 1 is for RX.
 */
typedef struct {
    int number;
    int value;
    int channel;
} CanFilter;

/* Public: The function definition for completely custom OpenXC command
 * handlers.
 *
 * name - the name field in the message received over USB.
 * value - the value of the message, parsed by the cJSON library and able to be
 *         read as a string, boolean, float or int.
 * event - an optional event, may be null if the OpenXC message didn't include
 *          it.
 * signals - The list of all signals.
 * signalCount - The length of the signals array.
 *
 * Returns true if the command caused something to be sent over CAN.
 */
typedef bool (*CommandHandler)(const char* name, cJSON* value, cJSON* event,
        CanSignal* signals, int signalCount);

/* Public: A command to read from USB and possibly write back to CAN.
 *
 * For completely customized CAN commands without a 1-1 mapping between an
 * OpenXC message from the host and a CAN signal, you can define the name of the
 * command and a custom function to handle it in the translator. An example is
 * the "turn_signal_status" command in OpenXC, which has a value of "left" or
 * "right". The vehicle may have separate CAN signals for the left and right
 * turn signals, so you will need to implement a custom command handler to
 *
 * genericName - The name of message received over USB.
 * handler - An function to actually process the recieved command's value
 *                and write it to CAN in the proper signals.
 */
typedef struct {
    const char* genericName;
    CommandHandler handler;
} CanCommand;

/* Public: Initialize the CAN controller. See inline comments for description of
 * the process.
 *
 * bus - A CanBus struct defining the bus's metadata for initialization.
 */
void initialize(CanBus* bus);

/* Public: De-initialize the CAN controller.
 *
 * bus - A CanBus struct defining the bus's metadata for initialization.
 */
void deinitialize(CanBus* bus);

/* Public: Perform platform-agnostic CAN initialization.
 */
void initializeCommon(CanBus* bus);

/* Public: Check if the device is connected to an active CAN bus, i.e. it's
 * received a message in the recent past.
 *
 * Returns true if a message was received on the CAN bus within
 * CAN_ACTIVE_TIMEOUT_S seconds.
 */
bool busActive(CanBus* bus);

/* Public: Look up the CanSignal representation of a signal based on its generic
 * name. The signal may or may not be writable - the first result will be
 * returned.
 *
 * name - The generic, OpenXC name of the signal.
 * signals - The list of all signals.
 * signalCount - The length of the signals array.
 *
 * Returns a pointer to the CanSignal if found, otherwise NULL.
 */
CanSignal* lookupSignal(const char* name, CanSignal* signals, int signalCount);

/* Public: Look up the CanSignal representation of a signal based on its generic
 * name.
 *
 * name - The generic, OpenXC name of the signal.
 * signals - The list of all signals.
 * signalCount - The length of the signals array.
 * writable - If true, only consider signals that are writable as candidates.
 *
 * Returns a pointer to the CanSignal if found, otherwise NULL.
 */
CanSignal* lookupSignal(const char* name, CanSignal* signals, int signalCount,
        bool writable);

/* Public: Look up the CanCommand representation of a command based on its
 * generic name.
 *
 * name - The generic, OpenXC name of the command.
 * commands - The list of all commands.
 * commandCount - The length of the commands array.
 *
 * Returns a pointer to the CanSignal if found, otherwise NULL.
 */
CanCommand* lookupCommand(const char* name, CanCommand* commands, int commandCount);

/* Public: Look up a CanSignalState for a CanSignal by its textual name. Use
 * this to find the numerical value to write back to CAN when a string state is
 * received from the user.
 *
 * name - The string name of the desired signal state.
 * signal - The CanSignal that should include this state.
 * signals - The list of all signals.
 * signalCount - The length of the signals array.
 *
 * Returns a pointer to the CanSignalState if found, otherwise NULL.
 */
CanSignalState* lookupSignalState(const char* name, CanSignal* signal,
        CanSignal* signals, int signalCount);

/* Public: Look up a CanSignalState for a CanSignal by its numerical value.
 * Use this to find the string equivalent value to write over USB when a float
 * value is received from CAN.
 *
 * value - the numerical value equivalent for the state.
 * name - The string name of the desired signal state.
 * signal - The CanSignal that should include this state.
 * signals - The list of all signals.
 * signalCount - The length of the signals array.
 *
 * Returns a pointer to the CanSignalState if found, otherwise NULL.
 */
CanSignalState* lookupSignalState(int value, CanSignal* signal,
        CanSignal* signals, int signalCount);

/* Public: Log transfer statistics about all active CAN buses to the debug log.
 *
 * buses - an array of active CAN buses.
 * busCount - the length of the buses array.
 */
void logBusStatistics(CanBus* buses, const int busCount);

} // can
} // openxc

#endif // _CANUTIL_H_
