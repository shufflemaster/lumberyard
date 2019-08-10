/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
// THIS CODE IS AUTOGENERATED, DO NOT MODIFY
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
using System.Net.Http;
using Newtonsoft.Json;
using System.Collections.Generic;

namespace {{json_object.namespace}} {
    {% for item in json_object.otherClasses %}
    {% if item.isArray %}
    public class {{ item.name }} : List<{{ item.elements }}>
    {
        public void AddRequestBody(JsonWriter jsonWriter)
        {
            jsonWriter.WriteStartArray();
            foreach (var thisElement in this)
            {
                {% if item.elements in ["string", "int", "bool", "double", "object"] %}
                jsonWriter.WriteValue(thisElement);
                {% else %}
                thisElement.AddRequestBody(jsonWriter);
                {% endif %}
            }
            jsonWriter.WriteEndArray();
        }

        public bool ParseResponse(JsonReader jsonReader)
        {
            while (jsonReader.Read())
            {
                if (jsonReader.TokenType == JsonToken.EndArray)
                {
                    break;
                }
                else if (jsonReader.TokenType == JsonToken.StartObject)
                {
                    {% if item.elements in ["string", "int", "bool", "double", "object"] %}
                    {{ item.elements }} nextObject;
                    {% if item.elements == "int" %}
                    nextObject = jsonReader.ReadAsInt32().Value;
                    {% elif item.elements == "bool" %}
                    nextObject = jsonReader.ReadAsBoolean().Value;
                    {% elif item.elements == "double" %}
                    nextObject = jsonReader.ReadAsDouble().Value;
                    {% elif item.elements == "string"  %}
                    nextObject = jsonReader.ReadAsString();
                    {% else %}
                    nextObject = jsonReader.Value;
                    {% endif %}
                    {% else %}
                    {{ item.elements }} nextObject = new {{ item.elements }}();
                    nextObject.ParseResponse(jsonReader);
                    {% endif %}
                    Add(nextObject);
                }
                {% if item.elements in ["string", "int", "bool", "double"] %}
                    {% if item.elements == "int" %}
                    else if (jsonReader.TokenType == JsonToken.Integer)
                    {% elif item.elements == "bool" %}
                    else if (jsonReader.TokenType == JsonToken.Boolean)                    
                    {% elif item.elements == "string"  %}
                    else if (jsonReader.TokenType == JsonToken.String)
                    {% elif item.elements == "double" %}
                    else if (jsonReader.Value is double || jsonReader.Value is int)                    
                    {% endif %}
                {
                    {{ item.elements }} nextObject;
                    nextObject = ({{ item.elements }})jsonReader.Value;
                    Add(nextObject);
                }
                {% endif %}
            }
            return true;
        }
    }
    {% else %}
    public class {{ item.name }}
    {
        {% for prop in item.props %}
        {% if prop.isArray %}
        public List<{{ prop.type }}> _{{prop.name}}{{prop.init}};
        {% else %}
        public {{ prop.type }} _{{prop.name}}{{prop.init}};
        {% endif %}
        {% endfor %}
        public {{ item.name }}()
        {
            {% for prop in item.props %}
            {% if prop.type not in ["string", "double", "int", "bool"] %}
            _{{ prop.name }} = new {{ prop.type }}();
            {% endif %}
            {% endfor %}
        }
        
        public void AddRequestBody(JsonWriter jsonWriter)
        {
            jsonWriter.WriteStartObject();
            {% for prop in item.props %}
            jsonWriter.WritePropertyName("{{prop.name}}");
            jsonWriter.WriteValue(_{{prop.name}});
            {% endfor %}
            jsonWriter.WriteEndObject();
        }

        public bool ParseResponse(JsonReader jsonReader)
        {
            while (jsonReader.Read())
            {
                if (jsonReader.TokenType == JsonToken.EndObject)
                {
                    break;
                }
                else if (jsonReader.TokenType == JsonToken.PropertyName)
                {
                    var propertyName = jsonReader.Value.ToString();
                    {% for prop in item.props %}
                    if (propertyName == "{{prop.name}}")
                    {
                        {% if prop.type == "int" %}
                        _{{prop.name}} = jsonReader.ReadAsInt32().Value;
                        {% elif prop.type == "bool" %}
                        _{{prop.name}} = jsonReader.ReadAsBoolean().Value;
                        {% elif prop.type == "string" %}
                        _{{prop.name}} = jsonReader.ReadAsString();
                        {% elif prop.type == "double" %}
                        _{{prop.name}} = jsonReader.ReadAsDouble().Value;
                        {% else %}
                        _{{prop.name}}.ParseResponse(jsonReader);
                        {% endif %}
                        continue;
                    }
                    {% endfor %}
                }
            }
            return true;
        }
    }
    {% endif %}
    {% endfor %}
    {% for path in json_object.functions %}
    public class {{ path.functionName}}Request : JsonServiceRequest
    {
        {% for param in path.params %}
        public {{ param.split(' ')[0]}} _{{ param.split(' ')[1]}};
        {% endfor %}
        {% if path.responseType %}
        public {{ path.responseType }} _Result;
        {% endif %}
        public {{ path.functionName}}Request() : base(HttpMethod.{{ path.http_method.lower().capitalize() }}, "{{ path.path }}")
        {
            {% for param in path.params %}
            {% if param.split(' ')[0] not in ["string", "double", "int", "bool"] %}
            _{{ param.split(' ')[1] }} = new {{ param.split(' ')[0]}}();
            {% endif %}
            {% endfor %}
            {% if path.responseType %}
            _Result = new {{ path.responseType }}();
            {% endif %}
        }
        {% if path.queryParamNames %}
        public override bool ResolveQueryParameters()
        {
            {% for param in path.queryParamNames %}
            AddQueryString("{{ param }}", _{{ param}});
            {% endfor %}
            return true;
        }
        {% endif %}
        {% if path.pathParamNames %}
        public override bool ResolvePathParameters()
        {
            {% for param in path.pathParamNames %}
            ReplacePathString("{{'{'}}{{ param }}{{'}'}}", _{{ param }});
            {% endfor %}
            return true;
        }
        {% endif %}
        {% if path.paramNames %}
        public override bool AddRequestBody()
        {
            JsonWriter jsonWriter = GetJsonWriter();
            {% for param in path.paramNames %}
            _{{ param }}.AddRequestBody(jsonWriter);
            {% endfor %}
            return true;
        }
        {% endif %}
        public override string GetLogicalServiceName()
        {
            return "{{json_object.resourceGroup}}.ServiceApi";
        }
        {% if path.responseType %}
        public override bool ParseResponse(JsonTextReader jsonReader)
        {
            _Result.ParseResponse(jsonReader);
            return true;
        }
        {% endif %}
    }
    {% endfor %}
}  // {{ json_object.namespace }}

