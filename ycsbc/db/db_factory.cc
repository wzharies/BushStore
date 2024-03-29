//
//  basic_db.cc
//  YCSB-C
//
//     on 12/17/14.
//   
//

#include "db/db_factory.h"

#include <string>
#include "db/basic_db.h"
#include "db/leveldb_db.h"

using namespace std;
using ycsbc::DB;
using ycsbc::DBFactory;

DB* DBFactory::CreateDB(utils::Properties &props) {
  if (props["dbname"] == "basic") {
    return new BasicDB;
  } else if (props["dbname"] == "leveldb") {
    std::string dbpath = props.GetProperty("dbpath","/tmp/test-leveldb");
    return new LevelDB(dbpath.c_str(), props);
  } else return NULL;
}

