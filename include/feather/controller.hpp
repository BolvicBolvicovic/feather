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
    
    /*- TemplateManager -*/
    /*
        Singleton that handles all the templates in the application.
    */
struct TemplateManager
{
    using TMInstance = std::shared_ptr<TemplateManager>;
    private:
        static inline TMInstance instance = nullptr;
        TemplateManager() = default;
    public:
        inja::Environment env;

        TemplateManager(TemplateManager const&) = delete;
        TemplateManager& operator=(TemplateManager const&) = delete;
        ~TemplateManager() = default;

        /*- fetch_instance -*/
        /*
            Request the instance of the singleton TemplateManager.
            If it is the first request, initialize the instance.
        */
        static TMInstance fetch_instance()
        {
            if (!instance)
            {
                instance = TMInstance(new TemplateManager());
            }
            return instance;
        }

        /*- add_template -*/
        /*
            Adds a template file in memory.
            All template files should be registred with this function.
        */
        static TMInstance add_template(TMInstance instance, std::string const& name, std::string const& path)
        {
            auto temp = instance->env.parse_template(path);
            instance->env.include_template(name, temp);
            return instance;
        }

        /*- render -*/
        /*
            Renders a template with the inja::Environment.
        */
        static std::string render(TMInstance instance, std::string const& name, json const& data)
        {
            return instance->env.render("{% include \""+ name + "\" %}", data);
        }

        /*- render_raw -*/
        /*
            Renders a template with the inja::Environment.
            Returns the raw template string.
        */
        static std::string render_raw(TMInstance instance, std::string const& temp, json const& data)
        {
            return instance->env.render(temp, data);
        }
        
};


    /*- render -*/
    /*
        Render a template and return the response.
    */
Conn const render(Conn const& conn, std::string const& template_name, json const& data, bool is_raw = false)
{
    try {
        if (is_raw)
        {
            auto body = TemplateManager::fetch_instance()
                CHAIN( TemplateManager::render_raw, template_name, data);
            return conn
                CHAIN( Conn::put_resp_header, "Content-Type", "text/html" )
                CHAIN( core::unwrap<Conn> )
                CHAIN( Conn::resp, 200, body );
        }
        auto body = TemplateManager::fetch_instance()
            CHAIN( TemplateManager::render, template_name, data);
        return conn
            CHAIN( Conn::put_resp_header, "Content-Type", "text/html" )
            CHAIN( core::unwrap<Conn> )
            CHAIN( Conn::resp, 200, body );
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