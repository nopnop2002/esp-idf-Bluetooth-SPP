typedef enum {
	CMD_OPEN,
	CMD_SEND,
	CMD_START,
	CMD_STOP,
	CMD_RECEIVE,
	CMD_CLOSE
} command_t;

typedef struct {
    uint32_t sppHandle;
    uint16_t command;
    size_t length;
    uint8_t payload[64];
    TaskHandle_t taskHandle;
} CMD_t;
