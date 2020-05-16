#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
import service

received_request = None
received_param_a = None
received_param_b = None
received_param_c = None
received_kwargs = None
response = None


def reset():
    global received_request, received_param_a, received_param_b, received_param_c, received_kwargs, response
    received_request = None
    received_param_a = None
    received_param_b = None
    received_param_c = None
    received_kwargs = None
    response = None

@service.api
def test_function_alone(request, param_a):
    global received_request, received_param_a, response
    received_request = request
    received_param_a = param_a
    return response

@service.api
def test_function_first(request, param_a, param_b):
    global received_request, received_param_a, received_param_b, response
    received_request = request
    received_param_a = param_a
    received_param_b = param_b
    return response

@service.api
def test_function_last(request, param_b, param_a):
    global received_request, received_param_a, received_param_b, response
    received_request = request
    received_param_a = param_a
    received_param_b = param_b
    return response

@service.api
def test_function_middle(request, param_b, param_a, param_c):
    global received_request, received_param_a, received_param_b, received_param_c, response
    received_request = request
    received_param_a = param_a
    received_param_b = param_b
    received_param_c = param_c
    return response

test_not_a_function = 1

def test_function_without_decorator(request, param_a):
    raise RuntimeError('Should not be called')

@service.api
def test_function_with_default_value(request, param_a = 'default param a'):
    global received_request, received_param_a, response
    received_request = request
    received_param_a = param_a
    return response

@service.api
def test_function_with_kwargs(request, param_a, **kwargs):
    global received_request, received_param_a, received_kwargs, response
    received_request = request
    received_param_a = param_a
    received_kwargs = kwargs
    return response


@service.api
def test_function_without_parameters(request):
    global received_request, response
    received_request = request
    return response


@service.api
def test_function_with_first_parameter_name_conflict(param_a, param_b):
    raise RuntimeError('Should not be called')


def __logging_filter(parameter):
    if isinstance(parameter, dict):
        param_a = parameter.get('param_a')
        if isinstance(param_a,  dict) and 'key1' in param_a:
            param_a['key1'] = 'REPLACEMENT_VALUE'


@service.api(unlogged_parameters=['param_b'], logging_filter=__logging_filter)
def test_function_with_parameter_logging_filter(request, param_a, param_b):
    global received_request, received_param_a, received_param_b, response
    received_request = request
    received_param_a = param_a
    received_param_b = param_b
    return response
