#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MSG_TYPE_REQ "REQ"
#define MSG_TYPE_RES "RES"
#define MSG_TYPE_UPD "UPD"
#define MSG_TYPE_ERR "ERR"
#define MSG_TYPE_HBT "HBT"

// JSON Actions
#define ACTION_LOGIN "LOGIN"
#define ACTION_REGISTER "REGISTER"
#define ACTION_LOGOUT "LOGOUT"
#define ACTION_CREATE_ROOM "CREATE_ROOM"
#define ACTION_LIST_ROOMS "LIST_ROOMS"
#define ACTION_IMPORT_QUESTIONS "IMPORT_QUESTIONS"
#define ACTION_LIST_QUESTION_BANKS "LIST_QUESTION_BANKS"
#define ACTION_GET_QUESTION_BANK "GET_QUESTION_BANK"
#define ACTION_GET_QUESTION_BANK "GET_QUESTION_BANK"
#define ACTION_UPDATE_QUESTION_BANK "UPDATE_QUESTION_BANK"
#define ACTION_DELETE_QUESTION_BANK "DELETE_QUESTION_BANK"
#define ACTION_GET_ROOM_STATS "GET_ROOM_STATS"
#define ACTION_CLOSE_ROOM "CLOSE_ROOM"
#define ACTION_DELETE_ROOM "DELETE_ROOM"

// JSON Field Keys
#define JSON_KEY_ACTION "action"
#define JSON_KEY_DATA "data"
#define JSON_KEY_USERNAME "username"
#define JSON_KEY_PASSWORD "password"
#define JSON_KEY_STATUS "status"
#define JSON_KEY_MESSAGE "message"

// Fixed header size = 4 bytes length + 3 bytes type = 7 bytes
// Fixed header size = 4 bytes length + 3 bytes type = 7 bytes
#define HEADER_SIZE 7
#define MAX_PAYLOAD_SIZE (128 * 1024)

typedef struct {
  uint32_t
      total_length;  // Includes header size + payload size (Network Byte Order)
  char msg_type[3];
} __attribute__((packed)) PacketHeader;

#endif
