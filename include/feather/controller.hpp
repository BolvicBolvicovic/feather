/*--- Header file for controller ---*/

#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <feather/core.hpp>
#include <inja.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>

namespace feather::controller
{
    using namespace feather::core;
    using namespace feather::core::plug;
    using json = nlohmann::json;
    using Controller = std::function<Conn(Conn const&, std::unordered_map<std::string, std::any>&&)>;
    

    /*- render -*/
    /*
        Render a template and return the response.
    */
    Conn const render(Conn const& conn, std::string const& template_name, json const& data)
    {
        try {
            // Check if template file exists
            if (!std::filesystem::exists(template_name)) {
                throw std::runtime_error("Template file not found: " + template_name);
            }

            // Read template file
            std::ifstream file(template_name);
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open template file: " + template_name);
            }
            std::string template_content((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
            file.close();

            // Create environment and parse template
            auto env = inja::Environment();
            
            // Configure environment for template inheritance
            env.set_expression("extends", "extends");
            env.set_expression("block", "block");
            env.set_expression("endblock", "endblock");
            
            // Set up template loader for inheritance
            env.set_include_callback([&template_name](const std::string& name, const std::string&) {
                try {
                    // Get the directory of the current template
                    std::filesystem::path template_path = std::filesystem::absolute(template_name);
                    std::filesystem::path base_path;
                    
                    std::cerr << "Template absolute path: " << template_path.string() << std::endl;
                    std::cerr << "Looking for base template: " << name << std::endl;
                    
                    // Handle relative paths
                    if (name.empty()) {
                        throw std::runtime_error("Base template name is empty");
                    }
                    
                    if (name.starts_with("./")) {
                        base_path = template_path.parent_path() / name.substr(2);
                    } else if (name.starts_with("/")) {
                        base_path = name;
                    } else {
                        base_path = template_path.parent_path() / name;
                    }
                    
                    std::cerr << "Base template absolute path: " << base_path.string() << std::endl;
                    std::cerr << "Base template exists: " << std::filesystem::exists(base_path) << std::endl;
                    if (std::filesystem::exists(base_path)) {
                        std::cerr << "Base template is directory: " << std::filesystem::is_directory(base_path) << std::endl;
                        std::cerr << "Base template is file: " << std::filesystem::is_regular_file(base_path) << std::endl;
                    }
                    
                    if (!std::filesystem::exists(base_path)) {
                        throw std::runtime_error("Base template not found: " + name + " at " + base_path.string());
                    }
                    
                    if (std::filesystem::is_directory(base_path)) {
                        throw std::runtime_error("Base template path is a directory: " + base_path.string());
                    }
                    
                    std::ifstream file(base_path);
                    if (!file.is_open()) {
                        throw std::runtime_error("Failed to open base template: " + name + " at " + base_path.string());
                    }
                    
                    std::string content((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
                    file.close();
                    
                    return inja::Environment().parse(content);
                } catch (const std::exception& e) {
                    throw std::runtime_error(std::string("Error in include callback: ") + e.what());
                }
            });
            
            try {
                auto tmpl = env.parse(template_content);
                
                // Render template
                auto rendered = env.render(tmpl, data);

                auto result = conn
                    CHAIN( Conn::put_resp_header, "Content-Type", "text/html" )
                    CHAIN( core::unwrap<Conn> )
                    CHAIN( Conn::resp, 200, rendered );
                return result;
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("Error during template parsing/rendering: ") + e.what());
            }
        } catch (const std::exception& e) {
            std::cerr << "Template rendering error: " << e.what() << std::endl;
            auto result = conn
                CHAIN( Conn::put_resp_header, "Content-Type", "text/plain" )
                CHAIN( core::unwrap<Conn> )
                CHAIN( Conn::resp, 500, std::string("Template rendering error: ") + e.what() );
            return result;
        }
    }

    /*- redirect -*/
    /*
        Redirect to a new URL.
    */
    Conn const redirect(Conn const& conn, std::string const& url)
    {
        return conn
            CHAIN( Conn::put_resp_header, "Location", url )
            CHAIN( core::unwrap<Conn> )
            CHAIN( Conn::resp, 302, "" );
    }

    /*- JSON -*/
    /*
        Return a JSON response.
    */
    Conn const JSON(Conn const& conn, json const& data)
    {
        return conn
            CHAIN( Conn::put_resp_header, "Content-Type", "application/json" )
            CHAIN( core::unwrap<Conn> )
            CHAIN( Conn::resp, 200, data.dump() );
    }

    /*- text -*/
    /*
        Return a text response.
    */
    Conn const text(Conn const& conn, std::string const& text)
    {
        return conn
            CHAIN( Conn::put_resp_header, "Content-Type", "text/plain" )
            CHAIN( core::unwrap<Conn> )
            CHAIN( Conn::resp, 200, text );
    }
    
}

#endif