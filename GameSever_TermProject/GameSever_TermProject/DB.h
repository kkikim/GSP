#pragma once
//-------------DBÀü¿ª-----------
#include <sqlext.h>
#define NAME_LEN 50  
#define PHONE_LEN 20  

SQLHENV henv;
SQLHDBC hdbc;
SQLHSTMT hstmt = 0;
SQLRETURN retcode;
SQLWCHAR szName[NAME_LEN];
SQLWCHAR szID[NAME_LEN];
char  buf[255];
char  buf2[255];
wchar_t sql_data[255];
SQLWCHAR sqldata[255] = { 0 };

SQLLEN cbName = 0, cbX = 0, cbY = 0, cbZ = 0, cbLevel = 0, cbHp = 0, cbRexp = 0, cbExp = 0;
SQLLEN cb_id = 0, cb_x = 0, cb_y = 0, cb_z = 0;