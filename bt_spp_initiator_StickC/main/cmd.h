#define CMD_OPEN    100
#define CMD_SEND    200
#define CMD_START   220
#define CMD_STOP    240
#define CMD_RECEIVE 300
#define CMD_CLOSE   400

typedef struct {
    uint32_t sppHandle;
    uint16_t command;
    size_t length;
    uint8_t payload[64];
    TaskHandle_t taskHandle;
} CMD_t;
