;#ifndef __MESSAGES_H__
;#define __MESSAGES_H__
;


LanguageNames =
    (
        English = 0x0409:Messages_ENU
    )


;////////////////////////////////////////
;// Eventlog categories
;//
;// Categories always have to be the first entries in a message file!
;//


MessageId       = 1
SymbolicName    = CATEGORY_ONE
Language        = English
Routing
.


;////////////////////////////////////////
;// Events
;//
MessageId       = 10
SymbolicName    = ROUTE_SET
Language        = English
Severity		= Success
Route %1 with MASK %2 and Gateway %3 added for %4 using file "%5"
.

MessageId       = +1
SymbolicName    = ROUTE_NOT_SET
Language        = English
Severity		= Error
Could not add route %1 with MASK %2 and Gateway %3 added for %4 using file "%5":
Error: %6
.

;
;#endif  //__MESSAGES_H__
;