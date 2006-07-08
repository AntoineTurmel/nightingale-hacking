/*
 //
// BEGIN SONGBIRD GPL
// 
// This file is part of the Songbird web player.
//
// Copyright� 2006 POTI, Inc.
// http://songbirdnest.com
// 
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the �GPL�).
// 
// Software distributed under the License is distributed 
// on an �AS IS� basis, WITHOUT WARRANTY OF ANY KIND, either 
// express or implied. See the GPL for the specific language 
// governing rights and limitations.
//
// You should have received a copy of the GPL along with this 
// program. If not, go to http://www.gnu.org/licenses/gpl.html
// or write to the Free Software Foundation, Inc., 
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
// 
// END SONGBIRD GPL
//
 */

//
// sbIPlaylist Object
//

const SONGBIRD_SIMPLEPLAYLIST_CONTRACTID = "@songbirdnest.com/Songbird/SimplePlaylist;1";
const SONGBIRD_SIMPLEPLAYLIST_CLASSNAME = "Songbird SimplePlaylist Interface"
const SONGBIRD_SIMPLEPLAYLIST_CID = Components.ID("{c5a2a53c-8ded-4043-9d5b-2747bc3a72ce}");
const SONGBIRD_SIMPLEPLAYLIST_IID = Components.interfaces.sbISimplePlaylist;

const SIMPLEPLAYLIST_LIST_TABLE_NAME = "simpleplaylist_list";

function CSimplePlaylist()
{
}

/* the CPlaylist class def */
CSimplePlaylist.prototype = 
{
  m_strName: "",
  m_strReadableName: "",
  m_queryObject: null,
  
  SetQueryObject: function(queryObj)
  {
    this.m_queryObject = queryObj; 
    return;
  },
  
  GetQueryObject: function()
  {
    return this.m_queryObject;
  },
  
  AddByURL: function(strURL, nColumnCount, aColumns, nValueCount, aValues, bWillRunLater)
  {
    if(this.m_queryObject != null)
    {
    
      var i = 0;
      var strQuery = "INSERT INTO \"" + this.m_strName + "\" (url, ";
      
      for(; i < nColumnCount; i++)
      {
        strQuery += aColumns[i];

        if(i != nColumnCount - 1)
          strQuery += ", ";
      }

      strQuery += ") VALUES (\"" + strURL + "\", ";

      for(i = 0; i < nValueCount; i++)
      {
        strQuery += "\"" + aValues[i] + "\"";

        if(i != nValueCount - 1)
          strQuery += ", ";
      }

      strQuery += ")";

      if(!bWillRunLater)
        this.m_queryObject.resetQuery();
        
      this.m_queryObject.addQuery(strQuery);

      if(!bWillRunLater)
      {
        this.m_queryObject.execute();
        this.m_queryObject.waitForCompletion();
      }
    }
    
    return;
  },
 
  RemoveByURL: function(strURL, bWillRunLater)
  {
    if(this.m_queryObject != null)
    {
      if(!bWillRunLater)
        this.m_queryObject.resetQuery();

      this.m_queryObject.addQuery("DELETE FROM \"" + this.m_strName + "\" WHERE url = \"" + strURL + "\"");
      
      if(!bWillRunLater)
      {
        this.m_queryObject.execute();
        this.m_queryObject.waitForCompletion();
      }
      
      return true;
    }
    
    return false;
  },
  
  RemoveByIndex: function(nIndex, bWillRunLater)
  {
    if(this.m_queryObject != null)
    {
      if(!bWillRunLater)
        this.m_queryObject.resetQuery();

      this.m_queryObject.addQuery("DELETE FROM \"" + this.m_strName + "\" WHERE id = \"" + nIndex + "\"");
      
      if(!bWillRunLater)
      {
        this.m_queryObject.execute();
        this.m_queryObject.waitForCompletion();
      }
      
      return true;
    }
    
    return false;  
  },

  RemoveByGUID: function(mediaGUID, bWillRunLater)
  {
    if(this.m_queryObject != null)
    {
      if(!bWillRunLater)
        this.m_queryObject.resetQuery();

      this.m_queryObject.addQuery("DELETE FROM \"" + this.m_strName + "\" WHERE playlist_uuid = \"" + mediaGUID + "\"");
      
      if(!bWillRunLater)
      {
        this.m_queryObject.execute();
        this.m_queryObject.waitForCompletion();
      }
      
      return true;
    }
    
    return false;
  },
  
  FindByURL: function(strURL)
  {
    if(this.m_queryObject != null)
    {
      this.m_queryObject.resetQuery();
      this.m_queryObject.addQuery("SELECT id FROM \"" + this.m_strName + "\" WHERE url = \"" + strURL + "\"");
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
      
      var resObj = this.m_queryObject.getResultObject();
      return resObj.GetRowCell(0, 0);
    }
    
    return -1;
  },
  
  FindByIndex: function(nIndex)
  {
    if(this.m_queryObject != null)
    {
      this.m_queryObject.resetQuery();
      this.m_queryObject.addQuery("SELECT url FROM \"" + this.m_strName + "\" WHERE id = \"" + nIndex + "\"");
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
      
      var resObj = this.m_queryObject.getResultObject();
      return resObj.GetRowCell(0, 0);
    }
    
    return "";
  },

  GetColumnInfo: function()
  {
    if(this.m_queryObject != null)
    {
      this.m_queryObject.resetQuery();
      this.m_queryObject.addQuery("SELECT * FROM \"" + this.m_strName + "_desc\"");
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
    }
  },
  
  SetColumnInfo: function(strColumn, strReadableName, isVisible, defaultVisibility, isMetadata, sortWeight, colWidth, bWillRunLater)
  {
    if(this.m_queryObject != null)
    {
      if(!bWillRunLater)
        this.m_queryObject.resetQuery();
      
      var strQuery = "UPDATE \"" + this.m_strName + "_desc\" SET ";
      strQuery += "readable_name = \"" + strReadableName + "\", ";
      strQuery += "is_visible = \"" + (isVisible == true ? "1" : "0") + "\", ";
      strQuery += "default_visibility = \"" + (defaultVisibility == true ? "1" : "0") + "\", ";
      strQuery += "is_metadata = \"" + (isMetadata == true ? "1" : "0") + "\", ";
      strQuery += "sort_weight = \"" + sortWeight + "\", ";
      strQuery += "width = \"" + colWidth + "\" ";
      strQuery += "WHERE column_name = \"" + strColumn + "\"";
      
      this.m_queryObject.addQuery(strQuery);
      
      if(!bWillRunLater)
      {
        this.m_queryObject.execute();
        this.m_queryObject.waitForCompletion();
      }
    }    
  },
  
  GetTableInfo: function()
  {
    if(this.m_queryObject != null)
    {
      this.m_queryObject.resetQuery();
      this.m_queryObject.addQuery("PRAGMA table_info(\"" + this.m_strName + "\")");
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
    }    
    
    return;
  },
  
  AddColumn: function(strColumn, strType)
  {
    if(this.m_queryObject != null)
    {
      this.m_queryObject.resetQuery();
      
      this.m_queryObject.addQuery("ALTER TABLE \"" + this.m_strName + "\" ADD COLUMN \"" + strColumn + "\" " + strType);
      this.m_queryObject.addQuery("INSERT OR REPLACE INTO \"" + this.m_strName + "_desc\" (column_name) VALUES (\"" + strColumn + "\")");
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
    }

    return;
  },
  
  DeleteColumn: function(strColumn)
  {
    if(this.m_queryObject != null)
    {
    }

    return;
  },

  GetNumEntries: function()
  {
    if(this.m_queryObject != null)
    {
      this.m_queryObject.resetQuery();
      this.m_queryObject.addQuery("SELECT COUNT(id) FROM \"" + this.m_strName + "\"");
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
      
      var resObj = this.m_queryObject.getResultObject();
      
      return resObj.GetRowCell(0, 0);
    }
    
    return 0;
  },  
  
  GetEntry: function(nEntry)
  {
    if(this.m_queryObject != null)
    {
      this.m_queryObject.resetQuery();
      this.m_queryObject.addQuery("SELECT * FROM \"" + this.m_strName + "\" WHERE id = \"" + nEntry + "\"");
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();

      return 1;
    }
    
    return 0;
  },

  GetAllEntries: function()
  {
    if(this.m_queryObject != null)
    {
      this.m_queryObject.resetQuery();
      this.m_queryObject.addQuery("SELECT * FROM \"" + this.m_strName + "\"");
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
      
      var resObj = this.m_queryObject.getResultObject();
      return resObj.GetRowCount();
    }
    
    return 0;
  },

  GetColumnValueByIndex: function(mediaIndex, strColumn)
  {
    if(this.m_queryObject != null)
    {
      this.m_queryObject.resetQuery();
      this.m_queryObject.addQuery("SELECT " + strColumn + " FROM \"" + this.m_strName + "\" WHERE id = \"" + mediaIndex + "\"");
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
      
      var resObj = this.m_queryObject.getResultObject();

      if(resObj.GetRowCount())
        return resObj.GetRowCell(0, 0);
    }

    return "";
  },
  
  GetColumnValueByURL: function(mediaURL, strColumn)
  {
    if(this.m_queryObject != null)
    {
      this.m_queryObject.resetQuery();
      this.m_queryObject.addQuery("SELECT " + strColumn + " FROM \"" + this.m_strName + "\" WHERE url = \"" + mediaURL + "\"");
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
      
      var resObj = this.m_queryObject.getResultObject();

      if(resObj.GetRowCount())
        return resObj.GetRowCell(0, 0);    
    }

    return "";
  },

  GetColumnValuesByIndex: function(mediaIndex, nColumnCount, aColumns, nValueCount)
  {
    nValueCount = 0;
    var aValues = new Array();

    if(this.m_queryObject != null)
    {
      var strQuery = "SELECT ";
      this.m_queryObject.resetQuery();
      
      var i = 0;
      for( ; i < nColumnCount; i++)
      {
        strQuery += aColumns[i];
        
        if(i < nColumnCount - 1)
          strQuery += ", ";
      }
      
      strQuery += " FROM " + this.m_strName + " WHERE id = \"" + mediaIndex + "\"";
      this.m_queryObject.addQuery(strQuery);
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
      
      var resObj = this.m_queryObject.getResultObject();
      nValueCount = resObj.GetColumnCount();
      
      for(var i = 0; i < nValueCount; i++)
      {
        aValues.push(resObj.GetRowCell(0, i));
      }    
    }
    
    return aValues;
  },
  
  GetColumnValuesByURL: function(mediaURL, nColumnCount, aColumns, nValueCount)
  {
    nValueCount = 0;
    var aValues = new Array();

    if(this.m_queryObject != null)
    {
      var strQuery = "SELECT ";
      this.m_queryObject.resetQuery();
      
      var i = 0;
      for( ; i < nColumnCount; i++)
      {
        strQuery += aColumns[i];
        
        if(i < nColumnCount - 1)
          strQuery += ", ";
      }
      
      strQuery += " FROM \"" + this.m_strName + "\" WHERE url = \"" + mediaURL + "\"";
      this.m_queryObject.addQuery(strQuery);
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
      
      var resObj = this.m_queryObject.getResultObject();
      nValueCount = resObj.GetColumnCount();
      
      for(var i = 0; i < nValueCount; i++)
      {
        aValues.push(resObj.GetRowCell(0, i));
      }    
    }
    
    return aValues;
  },
  
  SetColumnValueByIndex: function(mediaIndex, strColumn, strValue, bWillRunLater)
  {
    if(this.m_queryObject != null)
    {
      if(!bWillRunLater)
        this.m_queryObject.resetQuery();
      
      strValue = strValue.replace(/"/g, "\"\"");
      this.m_queryObject.addQuery("UPDATE \"" + this.m_strName + "\" SET " + strField + " = \"" + strValue + "\" WHERE id = \"" + mediaIndex + "\"");
      
      if(!bWillRunLater)
      {
        this.m_queryObject.execute();
        this.m_queryObject.waitForCompletion();
      }
    }
    
    return;
  },
  
  SetColumnValueByURL: function(mediaURL, strColumn, strValue, bWillRunLater)
  {
    if(this.m_queryObject != null)
    {
      if(!bWillRunLater)
        this.m_queryObject.resetQuery();
      
      strValue = strValue.replace(/"/g, "\"\"");
      this.m_queryObject.addQuery("UPDATE \"" + this.m_strName + "\" SET " + strColumn + " = \"" + strValue + "\" WHERE url = \"" + mediaURL + "\"");
      
      if(!bWillRunLater)
      {
        this.m_queryObject.execute();
        this.m_queryObject.waitForCompletion();
      }
    }
    
    return;
  },

  SetColumnValuesByIndex: function(mediaIndex, nColumnCount, aColumns, nValueCount, aValues, bWillRunLater)
  {
    if(this.m_queryObject != null ||
       nColumnCount != nValueCount)
    {
      if(!bWillRunLater)
        this.m_queryObject.resetQuery();
      
      var strQuery = "UPDATE " + this.m_strName + " SET ";
      var i = 0;
      for(; i < nColumnCount; i++)
      {
        aValues[i] = aValues[i].replace(/"/g, "\"\"");
        strQuery += aColumns[i] + " = \"" + aValues[i] + "\"";
        if(i < nColumnCount - 1)
          strQuery += ", ";
      }
      
      strQuery += " WHERE id = \"" + mediaIndex + "\"";
      this.m_queryObject.addQuery(strQuery);
      
      if(!bWillRunLater)
      {
        this.m_queryObject.execute();
        this.m_queryObject.waitForCompletion();
      }
    }

    return;
  },
  
  SetColumnValuesByURL: function(mediaURL, nColumnCount, aColumns, nValueCount, aValues, bWillRunLater)
  {
    if(this.m_queryObject != null ||
       nColumnCount != nValueCount)
    {
      if(!bWillRunLater)
        this.m_queryObject.resetQuery();
      
      var strQuery = "UPDATE \"" + this.m_strName + "\" SET ";
      var i = 0;
      for(; i < nColumnCount; i++)
      {
        aValues[i] = aValues[i].replace(/"/g, "\"\"");
        strQuery += aColumns[i] + " = \"" + aValues[i] + "\"";
        if(i < nColumnCount - 1)
          strQuery += ", ";
      }
      
      strQuery += " WHERE url = \"" + mediaURL + "\"";
      this.m_queryObject.addQuery(strQuery);
      
      if(!bWillRunLater)
      {
        this.m_queryObject.execute();
        this.m_queryObject.waitForCompletion();
      }
    }

    return;
  },

  SetName: function(strName)
  {
    if(strName != "")
      this.m_strName = strName;

    return;
  },
  
  GetName: function()
  {
    return this.m_strName;
  },
  
  SetReadableName: function(strReadableName)
  {
    if(this.m_queryObject != null)
    {
      this.m_queryObject.resetQuery();
      
      strReadableName = strReadableName.replace(/"/g, "\"\"");
      this.m_queryObject.addQuery("UPDATE " + SIMPLEPLAYLIST_LIST_TABLE_NAME + " SET readable_name = \"" + strReadableName + "\" WHERE name = \"" + this.m_strName + "\"");
      
      this.m_queryObject.execute();
      this.m_queryObject.waitForCompletion();
    }
        
    return;
  },
  
  GetReadableName: function()
  {
    var strReadableName = "";
    this.m_queryObject.resetQuery();
    this.m_queryObject.addQuery("SELECT readable_name FROM " + SIMPLEPLAYLIST_LIST_TABLE_NAME + " WHERE name = \"" + this.m_strName + "\"");
    
    this.m_queryObject.execute();
    this.m_queryObject.waitForCompletion();
    
    var resObj = this.m_queryObject.getResultObject();
    
    if(resObj.GetRowCount())
      strReadableName = resObj.GetRowCell(0, 0);    
    
    return strReadableName;
  },

  QueryInterface: function(iid)
  {
      if (!iid.equals(Components.interfaces.nsISupports) &&
          !iid.equals(SONGBIRD_SIMPLEPLAYLIST_IID))
          throw Components.results.NS_ERROR_NO_INTERFACE;
      return this;
  }
};

/**
 * \class sbSimplePlaylistModule
 * \brief 
 */
var sbSimplePlaylistModule = 
{
  registerSelf: function(compMgr, fileSpec, location, type)
  {
      compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
      compMgr.registerFactoryLocation(SONGBIRD_SIMPLEPLAYLIST_CID, 
                                      SONGBIRD_SIMPLEPLAYLIST_CLASSNAME, 
                                      SONGBIRD_SIMPLEPLAYLIST_CONTRACTID, 
                                      fileSpec, 
                                      location,
                                      type);
  },

  getClassObject: function(compMgr, cid, iid) 
  {
      if (!cid.equals(SONGBIRD_SIMPLEPLAYLIST_CID))
          throw Components.results.NS_ERROR_NO_INTERFACE;

      if (!iid.equals(Components.interfaces.nsIFactory))
          throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

      return sbSimplePlaylistFactory;
  },

  canUnload: function(compMgr)
  { 
    return true; 
  }
}; //sbSimplePlaylistModule

/**
 * \class sbSimplePlaylistFactory
 * \brief 
 */
var sbSimplePlaylistFactory =
{
    createInstance: function(outer, iid)
    {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;
    
        if (!iid.equals(SONGBIRD_SIMPLEPLAYLIST_IID) &&
            !iid.equals(Components.interfaces.nsISupports))
            throw Components.results.NS_ERROR_INVALID_ARG;

        return (new CSimplePlaylist()).QueryInterface(iid);
    }
}; //sbSimplePlaylistFactory

/**
 * \function NSGetModule
 * \brief 
 */
function NSGetModule(comMgr, fileSpec)
{ 
  return sbSimplePlaylistModule;
} //NSGetModule