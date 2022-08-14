module.exports = {

  "/ofp-api/v1/override": {
    "override": "cozyminus1"
  },

  "/ofp-api/v1/zones": {
    "zones": [
      {
        "id": "e1z1",
        "desc": "salon",
        "mode": ":planning:0",
        "current": "economy"
      },
      {
        "id": "e1z2",
        "desc": "chambre parents",
        "mode": ":fixed:nofreeze",
        "current": "nofreeze"
      },
      {
        "id": "e1z3",
        "desc": "e1z3",
        "mode": ":fixed:economy",
        "current": "economy"
      },
      {
        "id": "e1z4",
        "desc": "bureau cecile",
        "mode": ":fixed:cozyminus2",
        "current": "cozyminus2"
      },
      {
        "id": "e2z1",
        "desc": "e2z1",
        "mode": ":fixed:cozyminus1",
        "current": "cozyminus1"
      },
      {
        "id": "e2z2",
        "desc": "e2z2",
        "mode": ":fixed:cozy",
        "current": "cozy"
      },
      {
        "id": "e2z3",
        "desc": "salle de bain",
        "mode": ":planning:1",
        "current": "offload"
      },
      {
        "id": "e2z4",
        "desc": "inutilisé",
        "mode": ":fixed:offload",
        "current": "offload"
      }
    ]
  },

  "/ofp-api/v1/orders": {
    "orders": [
      {
        "id": "offload",
        "name": "Arr&ecirc;t / D&eacute;lestage",
        "class": "secondary"
      },
      {
        "id": "nofreeze",
        "name": "Hors-gel",
        "class": "info"
      },
      {
        "id": "economy",
        "name": "Economie",
        "class": "success"
      },
      {
        "id": "cozyminus1",
        "name": "Confort-1&deg;",
        "class": "warning"
      },
      {
        "id": "cozyminus2",
        "name": "Confort-2&deg;",
        "class": "warning"
      },
      {
        "id": "cozy",
        "name": "Confort",
        "class": "danger"
      }
    ]
  },

  "/ofp-api/v1/accounts":
  {
    "accounts": [
      {
        "id": "admin",
        "type": "admin"
      },
      {
        "id": "nico",
        "type": "user"
      },
      {
        "id": "cecile",
        "type": "user"
      }
    ]
  },

  "/ofp-api/v1/plannings": {
    "plannings": [
      {
        "id": 0,
        "name": "Pi&egrave;ces de vie"
      },
      {
        "id": 1,
        "name": "Bureaux"
      },
      {
        "id": 2,
        "name": "Chambres"
      }
    ]
  },

  "/ofp-api/v1/plannings/0": {
    "slots": [
      {
        "start": "00h00",
        "order": "nofreeze"
      },
      {
        "start": "05h00",
        "order": "economy"
      },
      {
        "start": "07h00",
        "order": "cozy"
      },
      {
        "start": "09h00",
        "order": "economy"
      },
      {
        "start": "17h00",
        "order": "cozy"
      },
      {
        "start": "21h00",
        "order": "nofreeze"
      }
    ]
  },

  "/ofp-api/v1/plannings/1": {
    "slots": [
      {
        "start": "00h00",
        "order": "economy"
      },
      {
        "start": "17h00",
        "order": "cozy"
      }
    ]
  },

  "/ofp-api/v1/plannings/2": {
    "slots": [
      {
        "start": "00h00",
        "order": "cozy"
      },
      {
        "start": "09h00",
        "order": "economy"
      },
      {
        "start": "10h00",
        "order": "nofreeze"
      }
    ]
  },

  "/ofp-api/v1/hardware": {
    "current": "M1E1",
    "supported": [
      {
        "id": "ESP32",
        "name": "modules ou DevKit seul"
      },
      {
        "id": "M1E1",
        "name": "DevKit NodeMCU 30 pin + OFP M1 + OFP E1"
      },
      {
        "id": "OFP-10Z",
        "name": "Carte 10 zones avec afficheur et bouton"
      }
    ]
  },
  "/ofp-api/v1/hardware/ESP32": {
    "parameters": [
    ]
  },

  "/ofp-api/v1/hardware/M1E1": {
    "parameters": [
      {
        "id": "ext_count",
        "description": "Nombre de cartes d'extension E1",
        "type": "number",
        "value": 3
      }
    ]
  },

  "/ofp-api/v1/hardware/OFP-10Z": {
    "parameters": [
      {
        "id": "dummy1",
        "description": "dummy number",
        "type": "number",
        "value": 69
      },
      {
        "id": "dummy2",
        "description": "dummy string",
        "type": "string",
        "value": "blah"
      }
    ]
  },

  "/ofp-api/v1/status": {
    "uptime": {
      "system": 43210,
      "wifi": 3679
    }
  },




















  "/customMockData/1": {
    "description": "Since '_config' flag is not set to true. This whole object will be considered as a mock data. If you don't have any specific route configuration you can directly give the mock data to the route or please make sure you provide '_config' to true to set any route configuration.",
    "mock": [
      {
        "userId": 1,
        "id": 1,
        "title": "Lorem ipsum dolor sit.",
        "body": "Lorem ipsum dolor sit amet consectetur adipisicing elit. Est, dolorem."
      },
      {
        "userId": 1,
        "id": 2,
        "title": "Lorem ipsum dolor sit.",
        "body": "Lorem ipsum dolor sit amet consectetur adipisicing elit. Est, dolorem."
      },
      {
        "userId": 1,
        "id": 3,
        "title": "Lorem ipsum dolor sit.",
        "body": "Lorem ipsum dolor sit amet consectetur adipisicing elit. Est, dolorem."
      },
      {
        "userId": 1,
        "id": 4,
        "title": "Lorem ipsum dolor sit.",
        "body": "Lorem ipsum dolor sit amet consectetur adipisicing elit. Est, dolorem."
      },
      {
        "userId": 1,
        "id": 5,
        "title": "Lorem ipsum dolor sit.",
        "body": "Lorem ipsum dolor sit amet consectetur adipisicing elit. Est, dolorem."
      }
    ]
  },
  "/customMockData/2": {
    "_config": true,
    "mock": [
      {
        "userId": 1,
        "id": 1,
        "title": "Lorem ipsum dolor sit.",
        "body": "Lorem ipsum dolor sit amet consectetur adipisicing elit. Est, dolorem."
      },
      {
        "userId": 1,
        "id": 2,
        "title": "Lorem ipsum dolor sit.",
        "body": "Lorem ipsum dolor sit amet consectetur adipisicing elit. Est, dolorem."
      },
      {
        "userId": 1,
        "id": 3,
        "title": "Lorem ipsum dolor sit.",
        "body": "Lorem ipsum dolor sit amet consectetur adipisicing elit. Est, dolorem."
      }
    ]
  },
  "/customDelay": {
    "_config": true,
    "delay": 2000,
    "description": "Note: give delay in milliseconds",
    "mock": "This is response is received with a delay of 2000 milliseconds"
  },
  "/customStatusCode": {
    "_config": true,
    "statusCode": 500,
    "mock": "This is response is received with a statusCode of 500"
  },
  "/fetch/photos/records/data": {
    "_config": true,
    "description": "By default fetch call will be made only once. Then it stores the value in the `fetchData` attribute and make the `fetchCount` to 0. When you again hit the url it will get the data from `fetchData` and returns as a response",
    "fetch": "http://jsonplaceholder.typicode.com/photos"
  },
  "/fetch/comments/proxy": {
    "_config": true,
    "description": "When you directly give the url, all the request query params and posted body data will also to sent to the given url",
    "fetch": "http://jsonplaceholder.typicode.com/comments",
    "fetchCount": -1
  },
  "/posts/:id?": {
    "_config": true,
    "fetch": "http://jsonplaceholder.typicode.com${req.url}",
    "fetchCount": -1
  },
  "/mockFirst": {
    "_config": true,
    "description": "This route sends the mock data first. If the mock attribute is undefined then It tries to fetch the data from the url. By default mockFirst is set to false.",
    "mockFirst": true,
    "mock": "This is response comes from mock attribute",
    "fetch": "http://jsonplaceholder.typicode.com/todos"
  },
  "/fetch/posts/customOptions/:id": {
    "_config": true,
    "description": "Give the `fetch` attribute as a axios request object. enclose the value with ${<variables>} to pass the req values",
    "fetch": {
      "method": "GET",
      "url": "http://jsonplaceholder.typicode.com/posts/${req.params.id}"
    },
    "fetchCount": 5
  },
  "/fetch/local/file": {
    "_config": true,
    "description": "The given fetch path will be relative to the root path given in config",
    "fetch": "./data/users.json"
  },
  "/fetch/local/image": {
    "_config": true,
    "description": "The given fetch path will be relative to the root path given in config",
    "fetch": "./data/mockserverlogo.png",
    "fetchCount": -1
  },
  "/fetch/image/url": {
    "_config": true,
    "fetch": "https://via.placeholder.com/600/771796"
  },
  "/fetch/todos/fetchCount/3/times": {
    "_config": true,
    "description": "By default the fetch will be called only one time. You can limit or extend the number of fetch calls using 'fetchCount' attribute",
    "fetch": "http://jsonplaceholder.typicode.com/todos",
    "fetchCount": 3
  },
  "/fetch/albums/fetchCount/Infinite/times": {
    "_config": true,
    "description": "Setting 'fetchCount' to -1 time will helps to make a fetch call on each and every url hit without any limit. By This way you always get a new fresh data from the fetch url.",
    "fetch": "http://jsonplaceholder.typicode.com/albums",
    "fetchCount": -1
  },
  "/fetch/404/skipFetchError": {
    "_config": true,
    "description": "Bu default fetch returns the actual error if occur. If you set `skipFetchError` flag to true. the If any error occur in fetch call it will then skips the fetch error and return you the mock data",
    "fetch": "http://localhost:3000/404",
    "skipFetchError": true,
    "mock": "This data is returned due to some error in fetch call. You can see the error in 'fetchError' attribute",
    "fetchCount": -1
  },
  "/fetch/users/customMiddleware": {
    "_config": true,
    "description": "Note: This middleware must be available in the 'middleware.js' by the below given names. This 'DataWrapper' will be called on every hit of this route.",
    "fetch": "http://jsonplaceholder.typicode.com/users",
    "middlewares": [
      "DataWrapper"
    ]
  },
  "/fetch/users/customMiddleware2": {
    "_config": true,
    "description": "Note: This middleware must be available in the 'middleware.js' by the below given names. This 'DataWrapper' will be called on every hit of this route.",
    "fetch": "http://jsonplaceholder.typicode.com/users",
    "middlewares": (req, res, next) => {
      res.locals.data = {
        status: "Success",
        message: "Retrieved Successfully",
        result: res.locals.data
      }
      next();
    }
  },
  "/addDirectMiddleware": (req, res) => {
    // this route will ignoreMiddlewareWrappers
    // In this route we cannot access res.locals.data or res.locals.getStore() etc...
    // This route is similar to /ignoreMiddlewareWrappers route given below
    res.send('This route cannot access res.locals.data or res.locals.getStore() etc... It ignores all helper middleware wrappers.');
  },
  "/ignoreMiddlewareWrappers": {
    "_config": true,
    "ignoreMiddlewareWrappers": true,
    "middlewares": (req, res) => {
      res.send('This route cannot access res.locals.data or res.locals.getStore() etc... It ignores all helper middleware wrappers.');
    }
  },
  "/middleware/utils/list": {
    "_config": true,
    "description": "These are the list of predefined middleware that are available for ease of use. For Example: This route uses the '_IterateResponse' middleware to iterate the mock data",
    "mock": [
      "_IterateResponse - Iterates to each response in an array for each hit. Note: the data must be an array",
      "_IterateRoutes - Iterates to each route in a mock data. Note: mock data contains the list of routes to be redirected",
      "_AdvancedSearch - This middleware helps to retrieve the data using filter, sort, pagination etc... Note: data must be an array of objects and contains a unique id",
      "_CrudOperation - Helps to do all the CRUD operations like Create, Read, Update, Delete using GET, PUT, PATCH, POST, DELETE method. Note: Id values are not mutable. Any id value in the body of your PUT or PATCH request will be ignored. Only a value set in a POST request will be respected, but only if not already take",
      "_FetchTillData - Helps to keep on make fetch calls until it gets a valid success data",
      "_SetFetchDataToMock - This middleware sets the 'fetchData' to 'mock' attribute. Note: The existing data in mock will be overridden",
      "_SetStoreDataToMock - This is similar to '_FetchTillData'. This sets the 'store' value to 'mock' attribute",
      "_MockOnly - Always send only the mock data the even If it has a 'fetchData'.",
      "_FetchOnly - Always send only the 'fetchData'",
      "_ReadOnly - Only GET methods calls are allowed."
    ]
  },
  "/middleware/utils/example/_IterateResponse": {
    "_config": true,
    "description": "This route iterates through each data. Try to hit again to see the data change. Note: The data must be of type array",
    "fetch": {
      "url": "http://jsonplaceholder.typicode.com/photos"
    },
    "middlewares": [
      "_IterateResponse"
    ]
  },
  "/middleware/utils/example/_IterateRoutes": {
    "_config": true,
    "description": "This route iterates through each route provide in the mock. Try to hit again to see the route change. Note: The data must be of type array",
    "mock": [
      "/injectors/1",
      "/injectors/2"
    ],
    "middlewares": [
      "_IterateRoutes"
    ]
  },
  "/middleware/utils/example/_AdvancedSearch/users/:id?": {
    "_config": true,
    "fetch": {
      "url": "http://jsonplaceholder.typicode.com/users"
    },
    "middlewares": [
      "_AdvancedSearch"
    ]
  },
  "/middleware/utils/example/_FetchTillData": {
    "_config": true,
    "description": "This route keeps on making fetch call until it gets a success data.",
    "fetch": "http://jsonplaceholder.typicode.com/dbs",
    "middlewares": [
      "_FetchTillData"
    ]
  },
  "/injectors/1": "I am from /injectors/1",
  "/injectors/2": "I am from /injectors/2",
  "/injectors/:id": {},
  "/getStoreValue": {
    "_config": true,
    "description": "This will call the below middleware and send the store value.",
    "middlewares": [
      "GetStoreValue"
    ]
  },
  "/internalRequest": {
    "_config": true,
    "description": "Gets the data from /customMockData/1 route",
    "fetch": "http://${config.host}:${config.port}/customMockData/1"
  },
  "/pageNotFound": {
    "_config": true,
    "fetch": "http://${config.host}:${config.port}/404",
    "fetchCount": -1
  }
}