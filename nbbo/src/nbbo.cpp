/*
 *    _____      _ ____  ____
 *   / ___/_____(_) __ \/ __ )
 *   \__ \/ ___/ / / / / __  |
 *  ___/ / /__/ / /_/ / /_/ / 
 * /____/\___/_/_____/_____/  
 *
 *
 * BEGIN_COPYRIGHT
 *
 * This file is part of SciDB.
 * Copyright (C) 2008-2014 SciDB, Inc.
 *
 * SciDB is free software: you can redistribute it and/or modify
 * it under the terms of the AFFERO GNU General Public License as published by
 * the Free Software Foundation.
 *
 * SciDB is distributed "AS-IS" AND WITHOUT ANY WARRANTY OF ANY KIND,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,
 * NON-INFRINGEMENT, OR FITNESS FOR A PARTICULAR PURPOSE. See
 * the AFFERO GNU General Public License for the complete license terms.
 *
 * You should have received a copy of the AFFERO GNU General Public License
 * along with SciDB.  If not, see <http://www.gnu.org/licenses/agpl-3.0.html>
 *
 * END_COPYRIGHT
 */
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "query/Operator.h"
#include "system/Exceptions.h"
#include "query/LogicalExpression.h"

#include <log4cxx/logger.h>

using namespace std;
using namespace scidb;

namespace nbbo
{

// Logger for operator. static to prevent visibility of variable outside of file
static log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("scidb.query.ops.nbbo"));

struct Order
{
  enum Type
  {
    NONE = 0,
    BUY = 1,
    SELL = 2
  };

  Type type;
  int32_t price; //in cents
  uint32_t volume;
  
  Order():
     type(NONE), price(0), volume(0)
  {}

  void reset()
  {
     type = NONE; 
     price = 0;
     volume = 0;
  }
}; 


class nbboAggregate : public Aggregate
{
private:
    map<uint64_t, Order>    _idToOrder;
    map<int, int>         _priceToBuyVolume;
    map<int, int>         _priceToSellVolume;
    string                _currentSymbol;
    bool                  _flag;
    map<int, int>::reverse_iterator _highestBuyIter;
    map<int, int>::iterator         _lowestSellIter;

public:
    nbboAggregate(const string& name, Type const& aggregateType):
        Aggregate(name, aggregateType, aggregateType),
        _flag(true)
    {
    }

    virtual AggregatePtr clone() const
    {
        return AggregatePtr(new nbboAggregate(getName(), getAggregateType()));
    }

    AggregatePtr clone(Type const& aggregateType) const
    {
        return AggregatePtr(new nbboAggregate(getName(), aggregateType));
    }

    bool ignoreNulls() const
    {
        return true;
    }

    Type getStateType() const
    {
        return getAggregateType();
    }

    void initializeState(Value& state)
    {
        state.setNull();
    }
    
    void innerIncorporateOrder(Order const& order, uint64_t const id, char action)
    {
        switch (action)
        {
        case 'A':   // Add an order
        {
           pair<map<uint64_t,Order>::iterator, bool> p = _idToOrder.insert(pair<uint64_t,Order>(id, order));
           if (p.second)
           {
               if (order.type == Order::BUY)
               {
                   int& volume = _priceToBuyVolume[order.price];
                   volume += order.volume;
                   if (_highestBuyIter == _priceToBuyVolume.rend() || _highestBuyIter->first < order.price )
                   {
                       _highestBuyIter = _priceToBuyVolume.rbegin();
                   }
               }
               else
               {
                   int& volume = _priceToSellVolume[order.price];
                   volume += order.volume; 
                   if (_lowestSellIter == _priceToSellVolume.end() || _lowestSellIter->first > order.price )
                   {
                      _lowestSellIter = _priceToSellVolume.begin();
                   }
               }
           }
           break;
        }
        case 'M':   // Modify an order
        {
          map<uint64_t, Order>::iterator orderIter = _idToOrder.find(id);
          if ( orderIter == _idToOrder.end())
          {
             break;
          }
          Order::Type type = orderIter->second.type;
          int32_t oldPrice = orderIter->second.price;
          int32_t oldVolume = orderIter->second.volume;
          if (type == Order::BUY)
          {
               map<int,int>::iterator iter = _priceToBuyVolume.find(oldPrice);
               iter->second -= oldVolume;
               if (iter->second <= 0)
                    _priceToBuyVolume.erase(iter);
          }
          else
          {
               map<int,int>::iterator iter = _priceToSellVolume.find(oldPrice);
               iter->second -= oldVolume;
               if (iter->second <= 0)
                    _priceToSellVolume.erase(iter);
          }
          orderIter->second.type = order.type;
	  orderIter->second.price = order.price;
          orderIter->second.volume = order.volume;
          if (order.type == Order::BUY)
          {
              int& volume = _priceToBuyVolume[order.price];
              volume += order.volume;
          }
          else
          {
              int& volume = _priceToSellVolume[order.price];
              volume += order.volume;
          }
          _highestBuyIter = _priceToBuyVolume.rbegin();
          _lowestSellIter = _priceToSellVolume.begin();
          break;
        }
        case 'D':   // Delete an order
        {
          map<uint64_t, Order>::iterator orderIter = _idToOrder.find(id);
          if ( orderIter == _idToOrder.end())
          {
             break;
          }
          Order::Type type = orderIter->second.type;
          int32_t price = orderIter->second.price;
          int32_t volume = orderIter->second.volume;
          if (type == Order::BUY)
          {
               map<int,int>::iterator iter = _priceToBuyVolume.find(price);
               iter->second -= volume;
               if (iter->second <= 0)
                    _priceToBuyVolume.erase(iter);
               _highestBuyIter = _priceToBuyVolume.rbegin();
          }
          else
          {
               map<int,int>::iterator iter = _priceToSellVolume.find(price);
               iter->second -= volume;
               if (iter->second <= 0)
                    _priceToSellVolume.erase(iter); 
               _lowestSellIter = _priceToSellVolume.begin();
          }
          _idToOrder.erase(orderIter);  
          break;
        }
        default:
          break;
        }
    }

    void incorporateOrder (string const& orderString)
    {
        stringstream ss(orderString);
        Order order;
        string orderField;
        uint64_t id;
        int counter = 0;
        char action;
        while (getline(ss, orderField, ','))
        {
          if (counter == 0)
          {
              action = orderField[0];
              if (action != 'A' && action != 'M' && action != 'D')
              {
                  ostringstream err; 
                  err<<"Encoutnered illegal action; action="<<action<<" orderField="<< orderField<< "orderString="<<orderString;
                  throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << err.str();
              }
          }
          else if (counter == 1)
          { 
             id = strtoull(orderField.c_str(), NULL, 10);
          }
          else if (counter == 2)
          {
             order.price = int32_t(100*atof(orderField.c_str()));
             if (order.price < 0)
                  throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Encountered negative price";
          }
          else if (counter == 3)
          {
             order.volume = atoi(orderField.c_str());
             if (order.volume < 0)
                  throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Encountered negative volume";
          }
          else if (counter == 4) 
          {  //symbol
          }
          else if (counter == 5)
          {
             if (orderField[0] == 'B')  
                 order.type = Order::BUY;
             else if (orderField[0] == 'S')
                 order.type = Order::SELL;
             else                             
                 throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Encountered illegal order type";
          } 
          else
          {
             throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Encountered an order with more than 6 fields";
          }
          ++counter;
        }
        innerIncorporateOrder(order, id, action);
    }   

    string getSymbol(string const& val)
    {
        stringstream ss(val);
        string symbol;
        int counter = 0;
        while (getline(ss, symbol, ','))
        {    
           if (counter == 4)
               return symbol;
           ++counter;
        }
    }

    void accumulate(Value& state, Value const& input)
    {
      string val = input.getString();
      string symbol = getSymbol(val);
      if(symbol!=_currentSymbol)
      {
          _priceToSellVolume.clear();
          _priceToBuyVolume.clear();
          _idToOrder.clear();
          _currentSymbol = symbol;
          _flag = true;
          _highestBuyIter = _priceToBuyVolume.rend();
          _lowestSellIter = _priceToSellVolume.end();
      }
      if (_flag == true)
      {
          _flag = false;
      }
      else
      {
          _flag =true;
          return;
      }
      stringstream ss(val);
      string item;
      vector<string> adds;
      vector<string> mods;
      vector<string> dels;
      while (getline(ss, item, '|'))
      {
          if (item[0]=='A')
              adds.push_back(item);
          else if (item[0] =='M')
              mods.push_back(item);
          else if (item[0] == 'D' )
              dels.push_back(item);
          else
              throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "Encountered a record with improper starting field";
      }
      for (size_t i =0; i<adds.size(); ++i)
      {
          incorporateOrder(adds[i]);
      }
      for (size_t i =0; i<mods.size(); ++i)
      {
          incorporateOrder(mods[i]);
      }
      for (size_t i =0; i<dels.size(); ++i)
      {
          incorporateOrder(dels[i]);
      }
    }

    void merge(Value& dstState, Value const& srcState)
    {
      throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << "All time must be in one chunk!";
    }

    void finalResult(Value& result, Value const& state)
    {  
        Order highestBuy;
        if (_highestBuyIter != _priceToBuyVolume.rend())
        {
            highestBuy.price = _highestBuyIter->first;
            highestBuy.volume = _highestBuyIter->second;
        }

        Order lowestSell;
        if (_lowestSellIter != _priceToSellVolume.end())
        {
            lowestSell.price = _lowestSellIter->first;
            lowestSell.volume = _lowestSellIter->second;
         }

        char buf[512]; 
        snprintf(buf, 512, "%f,%d,%f,%d", 
                 double(highestBuy.price) / 100, highestBuy.volume, 
                 double(lowestSell.price) / 100, lowestSell.volume);
        result.setString(buf);
    }

/** 
    void accumulate(Value& state, Value const& input)
    {
      string val = input.getString();
/* val must contain the comman-separated elements:
 *
 * type (either A D or M)
 * order id (treated as a string key)
 * price (converted to int)
 * volume (converted to int)
 * symbol 
 * ordertype (B for buy and S for sell)
 * /
      stringstream ss(val);
      string item;
      vector<string> v;
      while (getline(ss, item, ','))
      {
        v.push_back(item);
      }
//      if(v.size() < 6) return;  // check size for number of records
      if(v.at(4)!=current_symbol)
      {
fprintf(stderr, "Starting chunk for symbol %s\n",v.at(4).c_str());
          id_to_price.clear();
          id_to_vol.clear();
          price_to_id.clear();
          current_symbol = v.at(4);
      }
      string const& key = v.at(1);
      int price = int(100*atof(v.at(2).c_str()));
      int vol =  atoi(v.at(3).c_str());

      switch(v.at(0)[0])
      {
        case 'A':   // Add an order
        {
          pair<map<string,int>::iterator, bool> p = id_to_price.insert(pair<string,int>(key, price));
          p = id_to_vol.insert(pair<string, int> (key, vol));
          if (p.second)
          {
              price_to_id.insert(pair<int,string>(price, key));
          }
          break;
        }
        case 'M':   // Modify an order
        {
          map<string, int>::iterator price_iter = id_to_price.find(key);
          if ( price_iter == id_to_price.end())
          {
             break;
          }
          price = price_iter->second;
          multimap<int, string>::iterator iter = price_to_id.lower_bound(price);
          while ( iter != price_to_id.end() && iter->first == price)
          {
             if (iter->second == key)
             {
                 price_to_id.erase(iter);
                 break;
             }
             ++(iter);
          }
          id_to_price.erase(price_iter);
          id_to_vol.erase(key);
        
          pair<map<string,int>::iterator, bool> p = id_to_price.insert(pair<string,int>(key, price));
          p = id_to_vol.insert(pair<string, int> (key, vol));
          if (p.second)
          {
              price_to_id.insert(pair<int,string>(price, key));
          }
          break;
        }
        case 'D':   // Delete an order
        {
          map<string, int>::iterator price_iter = id_to_price.find(key);
          if ( price_iter == id_to_price.end())
          {
             break;
          }          
          price = price_iter->second;
          multimap<int, string>::iterator iter = price_to_id.lower_bound(price);
          while ( iter != price_to_id.end() && iter->first == price)
          {
             if (iter->second == key)
             { 
                 price_to_id.erase(iter);
                 break;
             }
             ++(iter);
          }
          id_to_price.erase(price_iter);
          id_to_vol.erase(key);
          break;
        }
        default:
          break;
      }
fprintf(stderr, "val=%s key=%s price=%d vol=%d id_to_price.size=%lu id_to_vol.size=%lu price_to_id.size=%lu\n",val.c_str(), key.c_str(), price, vol, id_to_price.size(), id_to_vol.size(), price_to_id.size());
    }
    
    void finalResult(Value& result, Value const& state)
    {
        char buf[512];
        int price=0, vol=0;
fprintf(stderr, "finalResult price_to_id.empty %d ",price_to_id.empty());
        if(!price_to_id.empty())
        {
          string const& key = price_to_id.rbegin()->second;
          string const& firstKey = price_to_id.begin()->second;
fprintf(stderr, " first=%s last=%s ",firstKey.c_str(), key.c_str()); 


          price = (id_to_price[key]);
          vol = (id_to_vol[key]);
fprintf(stderr, "price = %d vol=%d",price,vol);
        }
        snprintf(buf, 512, "%d,%d", price, vol);
fprintf(stderr, "\n");
        result.setString(buf);
    }
*/
};



vector<AggregatePtr> _aggregates;
EXPORTED_FUNCTION const vector<AggregatePtr>& GetAggregates()
{
    return _aggregates;
}

class nbboAggregateGeneratorInstance
{
public:
    nbboAggregateGeneratorInstance()
    {
        //Add new aggregates here:
        _aggregates.push_back(AggregatePtr(new nbboAggregate("nbbo", TypeLibrary::getType(TID_VOID))));

    }
} _aggregateGeneratorInstance;

}
