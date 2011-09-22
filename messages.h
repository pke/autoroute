#ifndef __MESSAGES_H__
#define __MESSAGES_H__

////////////////////////////////////////
// Eventlog categories
//
// Categories always have to be the first entries in a message file!
//
//
//  Values are 32 bit values laid out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//


//
// Define the severity codes
//


//
// MessageId: CATEGORY_ONE
//
// MessageText:
//
// Routing
//
#define CATEGORY_ONE                     0x00000001L

////////////////////////////////////////
// Events
//
//
// MessageId: ROUTE_SET
//
// MessageText:
//
// Severity		= Success
// Route %1 with MASK %2 and Gateway %3 added for %4 using file "%5"
//
#define ROUTE_SET                        0x0000000AL

//
// MessageId: ROUTE_NOT_SET
//
// MessageText:
//
// Severity		= Error
// Could not add route %1 with MASK %2 and Gateway %3 added for %4 using file "%5":
// Error: %6
//
#define ROUTE_NOT_SET                    0x0000000BL


#endif  //__MESSAGES_H__
