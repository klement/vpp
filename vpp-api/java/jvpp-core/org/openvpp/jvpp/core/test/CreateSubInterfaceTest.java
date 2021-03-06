/*
 * Copyright (c) 2016 Cisco and/or its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.openvpp.jvpp.core.test;

import static java.util.Objects.requireNonNull;

import org.openvpp.jvpp.JVpp;
import org.openvpp.jvpp.JVppRegistry;
import org.openvpp.jvpp.JVppRegistryImpl;
import org.openvpp.jvpp.core.JVppCoreImpl;
import org.openvpp.jvpp.core.dto.CreateSubif;
import org.openvpp.jvpp.core.dto.CreateSubifReply;
import org.openvpp.jvpp.core.dto.SwInterfaceDetailsReplyDump;
import org.openvpp.jvpp.core.dto.SwInterfaceDump;
import org.openvpp.jvpp.core.future.FutureJVppCoreFacade;

/**
 * <p>Tests sub-interface creation.<br> Equivalent to:<br>
 *
 * <pre>{@code
 * vppctl create sub GigabitEthernet0/9/0 1 dot1q 100 inner-dot1q any
 * }
 * </pre>
 *
 * To verify invoke:<br>
 * <pre>{@code
 * vpp_api_test json
 * vat# sw_interface_dump
 * }
 */
public class CreateSubInterfaceTest {

    private static SwInterfaceDump createSwInterfaceDumpRequest(final String ifaceName) {
        SwInterfaceDump request = new SwInterfaceDump();
        request.nameFilter = ifaceName.getBytes();
        request.nameFilterValid = 1;
        return request;
    }

    private static void requireSingleIface(final SwInterfaceDetailsReplyDump response, final String ifaceName) {
        if (response.swInterfaceDetails.size() != 1) {
            throw new IllegalStateException(
                    String.format("Expected one interface matching filter %s but was %d", ifaceName,
                            response.swInterfaceDetails.size()));
        }
    }

    private static CreateSubif createSubifRequest(final int swIfIndex, final int subId) {
        CreateSubif request = new CreateSubif();
        request.swIfIndex = swIfIndex; // super interface id
        request.subId = subId;
        request.noTags = 0;
        request.oneTag = 0;
        request.twoTags = 1;
        request.dot1Ad = 0;
        request.exactMatch = 1;
        request.defaultSub = 0;
        request.outerVlanIdAny = 0;
        request.innerVlanIdAny = 1;
        request.outerVlanId = 100;
        request.innerVlanId = 0;
        return request;
    }

    private static void print(CreateSubifReply reply) {
        System.out.printf("CreateSubifReply: %s\n", reply);
    }

    private static void testCreateSubInterface() throws Exception {
        System.out.println("Testing sub-interface creation using Java callback API");
        final JVppRegistry registry = new JVppRegistryImpl("CreateSubInterface");
        final JVpp jvpp = new JVppCoreImpl();
        final FutureJVppCoreFacade jvppFacade = new FutureJVppCoreFacade(registry, jvpp);

        System.out.println("Successfully connected to VPP");
        Thread.sleep(1000);

        final String ifaceName = "GigabitEthernet0/8/0";

        final SwInterfaceDetailsReplyDump swInterfaceDetails =
                jvppFacade.swInterfaceDump(createSwInterfaceDumpRequest(ifaceName)).toCompletableFuture().get();

        requireNonNull(swInterfaceDetails, "swInterfaceDump returned null");
        requireNonNull(swInterfaceDetails.swInterfaceDetails, "swInterfaceDetails is null");
        requireSingleIface(swInterfaceDetails, ifaceName);

        final int swIfIndex = swInterfaceDetails.swInterfaceDetails.get(0).swIfIndex;
        final int subId = 1;

        final CreateSubifReply createSubifReply =
                jvppFacade.createSubif(createSubifRequest(swIfIndex, subId)).toCompletableFuture().get();
        print(createSubifReply);

        final String subIfaceName = "GigabitEthernet0/8/0." + subId;
        final SwInterfaceDetailsReplyDump subIface =
                jvppFacade.swInterfaceDump(createSwInterfaceDumpRequest(subIfaceName)).toCompletableFuture().get();
        requireNonNull(swInterfaceDetails, "swInterfaceDump returned null");
        requireNonNull(subIface.swInterfaceDetails, "swInterfaceDump returned null");
        requireSingleIface(swInterfaceDetails, ifaceName);

        System.out.println("Disconnecting...");
        registry.close();
        Thread.sleep(1000);
    }

    public static void main(String[] args) throws Exception {
        testCreateSubInterface();
    }
}
