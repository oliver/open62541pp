#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "open62541pp/Node.h"
#include "open62541pp/Server.h"

using namespace opcua;

TEST_CASE("Node") {
    Server server;

    SECTION("Constructor") {
        REQUIRE_NOTHROW(Node(server, NodeId(0, UA_NS0ID_BOOLEAN)));
        REQUIRE_THROWS(Node(server, NodeId(0, "DoesNotExist")));
    }

    SECTION("Get/set node attributes") {
        auto node = server.getObjectsNode().addVariable({1, "testAttributes"}, "testAttributes");

        // get default attributes
        REQUIRE(node.getNodeClass() == NodeClass::Variable);
        REQUIRE(node.getBrowseName() == "testAttributes");
        REQUIRE(node.getDisplayName() == LocalizedText("", "testAttributes"));
        REQUIRE(node.getDescription().getText().empty());
        REQUIRE(node.getDescription().getLocale().empty());
        REQUIRE(node.getWriteMask() == 0);
        REQUIRE(node.getDataType() == NodeId(0, UA_NS0ID_BASEDATATYPE));
        REQUIRE(node.getValueRank() == ValueRank::Any);
        REQUIRE(node.getArrayDimensions().empty());
        REQUIRE(node.getAccessLevel() == UA_ACCESSLEVELMASK_READ);

        // set new attributes
        REQUIRE_NOTHROW(node.setDisplayName("en-US", "newDisplayName"));
        REQUIRE_NOTHROW(node.setDescription("de-DE", "newDescription"));
        REQUIRE_NOTHROW(node.setWriteMask(11));
        REQUIRE_NOTHROW(node.setDataType(NodeId(0, 2)));
        REQUIRE_NOTHROW(node.setValueRank(ValueRank::TwoDimensions));
        REQUIRE_NOTHROW(node.setArrayDimensions({3, 2}));
        REQUIRE_NOTHROW(node.setAccessLevel(UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE));

        // get new attributes
        REQUIRE(node.getDisplayName() == LocalizedText("en-US", "newDisplayName"));
        REQUIRE(node.getDescription() == LocalizedText("de-DE", "newDescription"));
        REQUIRE(node.getWriteMask() == 11);
        REQUIRE(node.getDataType() == NodeId(0, 2));
        REQUIRE(node.getValueRank() == ValueRank::TwoDimensions);
        REQUIRE(node.getArrayDimensions().size() == 2);
        REQUIRE(node.getArrayDimensions().at(0) == 3);
        REQUIRE(node.getArrayDimensions().at(1) == 2);
        REQUIRE(node.getAccessLevel() == (UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE));
    }

    SECTION("Value rank and array dimension combinations") {
        auto node = server.getObjectsNode().addVariable({1, "testDimensions"}, "testDimensions");

        SECTION("Unspecified dimension (ValueRank <= 0)") {
            auto valueRank = GENERATE(
                ValueRank::Any,
                ValueRank::Scalar,
                ValueRank::ScalarOrOneDimension,
                ValueRank::OneOrMoreDimensions
            );
            CAPTURE(valueRank);
            node.setValueRank(valueRank);
            CHECK_NOTHROW(node.setArrayDimensions({}));
            CHECK_THROWS(node.setArrayDimensions({1}));
            CHECK_THROWS(node.setArrayDimensions({1, 2}));
            CHECK_THROWS(node.setArrayDimensions({1, 2, 3}));
        }

        SECTION("OneDimension") {
            node.setValueRank(ValueRank::OneDimension);
            node.setArrayDimensions({1});
            CHECK_THROWS(node.setArrayDimensions({}));
            CHECK_THROWS(node.setArrayDimensions({1, 2}));
            CHECK_THROWS(node.setArrayDimensions({1, 2, 3}));
        }

        SECTION("TwoDimensions") {
            node.setValueRank(ValueRank::TwoDimensions);
            node.setArrayDimensions({1, 2});
            CHECK_THROWS(node.setArrayDimensions({}));
            CHECK_THROWS(node.setArrayDimensions({1}));
            CHECK_THROWS(node.setArrayDimensions({1, 2, 3}));
        }

        SECTION("ThreeDimensions") {
            node.setValueRank(ValueRank::ThreeDimensions);
            node.setArrayDimensions({1, 2, 3});
            CHECK_THROWS(node.setArrayDimensions({}));
            CHECK_THROWS(node.setArrayDimensions({1}));
            CHECK_THROWS(node.setArrayDimensions({1, 2}));
        }
    }

    SECTION("Node class of default nodes") {
        CHECK(server.getRootNode().getNodeClass() == NodeClass::Object);
        CHECK(server.getObjectsNode().getNodeClass() == NodeClass::Object);
        CHECK(server.getTypesNode().getNodeClass() == NodeClass::Object);
        CHECK(server.getViewsNode().getNodeClass() == NodeClass::Object);
        CHECK(server.getObjectTypesNode().getNodeClass() == NodeClass::Object);
        CHECK(server.getVariableTypesNode().getNodeClass() == NodeClass::Object);
        CHECK(server.getDataTypesNode().getNodeClass() == NodeClass::Object);
        CHECK(server.getReferenceTypesNode().getNodeClass() == NodeClass::Object);
    }

    SECTION("Get child") {
        REQUIRE_THROWS(server.getRootNode().getChild({}));
        REQUIRE_THROWS(server.getRootNode().getChild({{0, "Invalid"}}));
        REQUIRE(
            server.getRootNode().getChild({{0, "Types"}, {0, "ObjectTypes"}}) ==
            server.getObjectTypesNode()
        );
    }

    SECTION("Try read/write with node classes other than Variable") {
        REQUIRE_THROWS(server.getRootNode().readScalar<int>());
        REQUIRE_THROWS(server.getRootNode().writeScalar<int>({}));
    }

    SECTION("Read/write data value") {
        auto node = server.getRootNode().addVariable({1, "testValue"}, "testValue");

        Variant var;
        var.setScalarCopy<int>(11);
        DataValue valueWrite(var, {}, {DateTime::now()}, {}, 1, UA_STATUSCODE_GOOD);
        node.writeDataValue(valueWrite);

        DataValue valueRead;
        node.readDataValue(valueRead);

        CHECK(valueRead->hasValue);
        CHECK(valueRead->hasServerTimestamp);
        CHECK(valueRead->hasSourceTimestamp);
        CHECK(valueRead->hasServerPicoseconds);
        CHECK(valueRead->hasSourcePicoseconds);
        CHECK_FALSE(valueRead->hasStatus);  // doesn't contain error code on success

        CHECK(valueRead.getValue().value().getScalar<int>() == 11);
        CHECK(valueRead->sourceTimestamp == valueWrite->sourceTimestamp);
        CHECK(valueRead->sourcePicoseconds == valueWrite->sourcePicoseconds);
    }

    SECTION("Read/write scalar") {
        auto node = server.getRootNode().addVariable({1, "testScalar"}, "testScalar");
        node.setDataType(Type::Float);

        // Writes with wrong data type
        REQUIRE_THROWS(node.writeScalar<bool>({}));
        REQUIRE_THROWS(node.writeScalar<int>({}));

        // Writes with correct data type
        float value = 11.11f;
        REQUIRE_NOTHROW(node.writeScalar(value));
        REQUIRE(node.readScalar<float>() == value);
    }

    SECTION("Read/write string") {
        auto node = server.getRootNode().addVariable({1, "testString"}, "testString");
        node.setDataType(Type::String);

        String str("test");
        REQUIRE_NOTHROW(node.writeScalar(str));
        REQUIRE(node.readScalar<std::string>() == "test");
    }

    SECTION("Read/write array") {
        auto node = server.getRootNode().addVariable({1, "testArray"}, "testArray");
        node.setDataType(Type::Double);

        // Writes with wrong data type
        REQUIRE_THROWS(node.writeArray<int>({}));
        REQUIRE_THROWS(node.writeArray<float>({}));

        // Writes with correct data type
        std::vector<double> array{11.11, 22.22, 33.33};

        SECTION("Write as std::vector") {
            REQUIRE_NOTHROW(node.writeArray(array));
            REQUIRE(node.readArray<double>() == array);
        }

        SECTION("Write as raw array") {
            REQUIRE_NOTHROW(node.writeArray(array.data(), array.size()));
            REQUIRE(node.readArray<double>() == array);
        }

        SECTION("Write as iterator pair") {
            REQUIRE_NOTHROW(node.writeArray(array.begin(), array.end()));
            REQUIRE(node.readArray<double>() == array);
        }
    }

    SECTION("Remove node") {
        const NodeId id(1, "testObj");

        auto node = server.getObjectsNode().addObject(id, "obj");
        REQUIRE_NOTHROW(Node(server, id));

        node.remove();
        REQUIRE_THROWS(Node(server, id));
    }

    SECTION("Equality") {
        REQUIRE(server.getRootNode() == server.getRootNode());
        REQUIRE(server.getRootNode() != server.getObjectsNode());
    }
}
