/*--- Code file for test controller ---*/

#include "test_pch.hpp"
#include <feather/controller.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include "test_utils.hpp"
#include <stdexcept>

using namespace feather::controller;
using namespace feather::core;

// Helper function to create a temporary test template
static std::string create_test_template(const std::string& content) {
    try {
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
        std::filesystem::path template_path = temp_dir / "test_template.html";
        
        // Ensure the temp directory exists and is writable
        if (!std::filesystem::exists(temp_dir)) {
            throw std::runtime_error("Temp directory does not exist");
        }
        
        // Create a unique filename to avoid conflicts
        std::string unique_filename = "test_template_" + std::to_string(std::time(nullptr)) + ".html";
        template_path = temp_dir / unique_filename;
        
        // Open file with error checking
        std::ofstream file(template_path, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open template file: " + template_path.string());
        }
        
        // Write content and ensure it's flushed
        file << content;
        file.flush();
        file.close();
        
        // Verify file was created and is readable
        if (!std::filesystem::exists(template_path)) {
            throw std::runtime_error("Template file was not created");
        }
        
        return template_path.string();
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create test template: ") + e.what());
    }
}

TEST_CASE("Controller response functions", "[controller]") {
    SECTION("render function - basic variables") {
        auto conn = test::buildFirstConn();
        json data = {
            {"title", "Test Page"},
            {"content", "Hello World"},
            {"number", 42},
            {"is_active", true}
        };
        
        std::string template_content = R"(
            <h1>{{ title }}</h1>
            <p>{{ content }}</p>
            <span>{{ number }}</span>
            <div>{% if is_active %}Active{% else %}Inactive{% endif %}</div>
        )";
        
        std::string template_path;
        try {
            template_path = create_test_template(template_content);
            auto result = render(conn, template_path, data);
            
            REQUIRE(result.status.value() == 200);
            auto content_type_range = Conn::get_resp_header(result, "content-type");
            REQUIRE(content_type_range.first->second == "text/html");
            REQUIRE(result.resp_body->find("<h1>Test Page</h1>") != std::string::npos);
            REQUIRE(result.resp_body->find("<p>Hello World</p>") != std::string::npos);
            REQUIRE(result.resp_body->find("<span>42</span>") != std::string::npos);
            REQUIRE(result.resp_body->find("<div>Active</div>") != std::string::npos);
        } catch (const std::exception& e) {
            FAIL("Test failed with exception: " << e.what());
        }
        
        // Clean up
        try {
            if (!template_path.empty() && std::filesystem::exists(template_path)) {
                std::filesystem::remove(template_path);
            }
        } catch (const std::exception& e) {
            // Log cleanup failure but don't fail the test
            std::cerr << "Warning: Failed to clean up template file: " << e.what() << std::endl;
        }
    }

    SECTION("render function - loops and arrays") {
        auto conn = test::buildFirstConn();
        std::vector<std::string> items = {"Item 1", "Item 2", "Item 3"};
        json data = {
            {"items", items}
        };
        
        std::string template_content = R"(
            <ul>
            {% for item in items %}
                <li>{{ item }}</li>
            {% endfor %}
            </ul>
        )";
        
        auto template_path = create_test_template(template_content);
        auto result = render(conn, template_path, data);
        
        REQUIRE(result.status.value() == 200);
        REQUIRE(result.resp_body->find("<li>Item 1</li>") != std::string::npos);
        REQUIRE(result.resp_body->find("<li>Item 2</li>") != std::string::npos);
        REQUIRE(result.resp_body->find("<li>Item 3</li>") != std::string::npos);
        
        std::filesystem::remove(template_path);
    }

    SECTION("render function - nested objects") {
        auto conn = test::buildFirstConn();
        json user = {
            {"name", "John Doe"},
            {"age", 30},
            {"address", json{
                {"street", "123 Main St"},
                {"city", "Anytown"}
            }}
        };
        
        json data = {
            {"user", user}
        };
        
        std::string template_content = R"(
            <div class="user">
                <h2>{{ user.name }}</h2>
                <p>Age: {{ user.age }}</p>
                <p>Address: {{ user.address.street }}, {{ user.address.city }}</p>
            </div>
        )";
        
        auto template_path = create_test_template(template_content);
        auto result = render(conn, template_path, data);
        
        REQUIRE(result.status.value() == 200);
        REQUIRE(result.resp_body->find("<h2>John Doe</h2>") != std::string::npos);
        REQUIRE(result.resp_body->find("Age: 30") != std::string::npos);
        REQUIRE(result.resp_body->find("Address: 123 Main St, Anytown") != std::string::npos);
        
        std::filesystem::remove(template_path);
    }

    SECTION("render function - conditional rendering") {
        auto conn = test::buildFirstConn();
        json data = {
            {"is_admin", true},
            {"has_permissions", false}
        };
        
        std::string template_content = R"(
            {% if is_admin %}
                <div class="admin-panel">Admin Panel</div>
            {% endif %}
            {% if has_permissions %}
                <div class="permissions">User Permissions</div>
            {% else %}
                <div class="no-permissions">No Permissions</div>
            {% endif %}
        )";
        
        auto template_path = create_test_template(template_content);
        auto result = render(conn, template_path, data);
        
        REQUIRE(result.status.value() == 200);
        REQUIRE(result.resp_body->find("<div class=\"admin-panel\">Admin Panel</div>") != std::string::npos);
        REQUIRE(result.resp_body->find("<div class=\"no-permissions\">No Permissions</div>") != std::string::npos);
        
        std::filesystem::remove(template_path);
    }

    SECTION("render function - template inheritance") {
        auto conn = test::buildFirstConn();
        json data = {
            {"title", "Child Page"},
            {"content", "This is the child content"}
        };
        
        // Create base template
        std::string base_template = R"(
            <!DOCTYPE html>
            <html>
            <head>
                <title>{{ title }}</title>
            </head>
            <body>
                <header>{% block header %}{% endblock %}</header>
                <main>{% block content %}{% endblock %}</main>
            </body>
            </html>
        )";
        
        // Create templates in the same directory with predictable names
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
        std::string unique_prefix = "test_template_" + std::to_string(std::time(nullptr));
        std::filesystem::path base_path = temp_dir / "base.html";
        std::filesystem::path child_path = temp_dir / (unique_prefix + "_child.html");
        
        try {
            std::cerr << "Temp directory: " << temp_dir.string() << std::endl;
            std::cerr << "Base template path: " << base_path.string() << std::endl;
            std::cerr << "Child template path: " << child_path.string() << std::endl;
            
            // Create base template
            std::ofstream base_file(base_path, std::ios::out | std::ios::trunc);
            if (!base_file.is_open()) {
                throw std::runtime_error("Failed to open base template file");
            }
            base_file << base_template;
            base_file.close();
            
            // Verify base template was created
            if (!std::filesystem::exists(base_path)) {
                throw std::runtime_error("Base template file was not created");
            }
            if (std::filesystem::is_directory(base_path)) {
                throw std::runtime_error("Base template path is a directory");
            }
            
            // Create child template with explicit base template path
            std::string child_template_with_path = R"(
                {% extends "./base.html" %}
                {% block header %}
                    <h1>{{ title }}</h1>
                {% endblock %}
                {% block content %}
                    <div>{{ content }}</div>
                {% endblock %}
            )";
            
            std::ofstream child_file(child_path, std::ios::out | std::ios::trunc);
            if (!child_file.is_open()) {
                throw std::runtime_error("Failed to open child template file");
            }
            child_file << child_template_with_path;
            child_file.close();
            
            // Verify child template was created
            if (!std::filesystem::exists(child_path)) {
                throw std::runtime_error("Child template file was not created");
            }
            if (std::filesystem::is_directory(child_path)) {
                throw std::runtime_error("Child template path is a directory");
            }
            
            // Verify both files are in the same directory
            if (base_path.parent_path() != child_path.parent_path()) {
                throw std::runtime_error("Base and child templates must be in the same directory");
            }
            
            auto result = render(conn, child_path.string(), data);
            
            REQUIRE(result.status.value() == 200);
            REQUIRE(result.resp_body->find("<title>Child Page</title>") != std::string::npos);
            REQUIRE(result.resp_body->find("<h1>Child Page</h1>") != std::string::npos);
            REQUIRE(result.resp_body->find("<div>This is the child content</div>") != std::string::npos);
        } catch (const std::exception& e) {
            FAIL("Test failed with exception: " << e.what());
        }
        
        // Clean up
        try {
            if (std::filesystem::exists(base_path)) {
                std::filesystem::remove(base_path);
            }
            if (std::filesystem::exists(child_path)) {
                std::filesystem::remove(child_path);
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to clean up template files: " << e.what() << std::endl;
        }
    }

    SECTION("redirect function") {
        auto conn = test::buildFirstConn();
        std::string url = "https://example.com";
        
        auto result = redirect(conn, url);
        
        REQUIRE(result.status.value() == 302);
        auto location_range = Conn::get_resp_header(result, "location");
        REQUIRE(location_range.first->second == url);
    }

    SECTION("json function") {
        auto conn = test::buildFirstConn();
        json data = {
            {"key", "value"}
        };
        
        auto result = JSON(conn, data);
        
        REQUIRE(result.status.value() == 200);
        auto content_type_range = Conn::get_resp_header(result, "content-type");
        REQUIRE(content_type_range.first->second == "application/json");
        REQUIRE(*result.resp_body == data.dump());
    }

    SECTION("text function") {
        auto conn = test::buildFirstConn();
        std::string text_data = "Plain text response";
        
        auto result = text(conn, text_data);
        
        REQUIRE(result.status.value() == 200);
        auto content_type_range = Conn::get_resp_header(result, "content-type");
        REQUIRE(content_type_range.first->second == "text/plain");
        REQUIRE(*result.resp_body == text_data);
    }
}

TEST_CASE("Controller chaining", "[controller]") {
    SECTION("Multiple header modifications") {
        auto conn = test::buildFirstConn();
        http::Headers headers = {
            {"x-custom-header", "test"},
            {"x-another-header", "value"}
        };
        
        auto result = conn
            CHAIN(Conn::merge_resp_headers, headers)
            CHAIN(unwrap<Conn>)
            CHAIN(Conn::resp, 200, "test");
            
        REQUIRE(result.status.value() == 200);
        auto custom_header_range = Conn::get_resp_header(result, "x-custom-header");
        auto another_header_range = Conn::get_resp_header(result, "x-another-header");
        REQUIRE(custom_header_range.first->second == "test");
        REQUIRE(another_header_range.first->second == "value");
    }
}

TEST_CASE("Controller error handling", "[controller]") {
    SECTION("Invalid template rendering") {
        auto conn = test::buildFirstConn();
        json data;
        
        auto result = render(conn, "nonexistent_template.html", data);
        
        REQUIRE(result.status.value() == 500);
        auto content_type_range = Conn::get_resp_header(result, "content-type");
        REQUIRE(content_type_range.first->second == "text/plain");
        REQUIRE(result.resp_body->find("Template rendering error: Template file not found") != std::string::npos);
    }
}