#include "mysqlOperator.hpp"

namespace mysqlOperator
{
    mysqlOperator::mysqlOperator()
    {
        PLOGI << "starting to initialize the mysql running envrionment";
        if (!mysql_init(&this->mysql))
        {
            PLOGE << "mysql running environment initiailzing failed!";
            exit(EXIT_FAILURE);
        }
        PLOGI << "mysql running environment initializing succeed.";
    }

    mysqlOperator::~mysqlOperator()
    {
        try
        {
            mysql_close(&this->mysql);
        }
        catch (const std::exception &e)
        {
            PLOGE << "mysql handle close make error, exit the program!" << e.what();
            exit(EXIT_FAILURE);
        }
    }
} // namespace mysqlOperator