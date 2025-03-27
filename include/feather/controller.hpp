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

    /*- accepts -*/
    /*
        Return true if the client accepts the given mime types.
    */
    bool accepts(Conn const& conn, s_list const& mime_types)
    {
        return conn.req_headers.find("Accept") != conn.req_headers.end()
            && conn.req_headers.at("Accept").find(mime_types) != std::string::npos;
    }

    /*- put_secure_browser_headers -*/
    /*
        Put the secure browser headers to the response.
    */
    Conn const put_secure_browser_headers(Conn const& conn)
    {
        return conn
            CHAIN( Conn::put_resp_header, "X-Frame-Options", "SAMEORIGIN" )
            CHAIN( Conn::put_resp_header, "X-XSS-Protection", "1; mode=block" )
            CHAIN( Conn::put_resp_header, "X-Content-Type-Options", "nosniff" )
            CHAIN( Conn::put_resp_header, "Referrer-Policy", "strict-origin-when-cross-origin" )
            CHAIN( Conn::put_resp_header, "Content-Security-Policy", "default-src 'self'; img-src *; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; connect-src 'self' https://fonts.googleapis.com https://fonts.gstatic.com; font-src 'self' https://fonts.googleapis.com https://fonts.gstatic.com; frame-src 'none'" )
            CHAIN( Conn::put_resp_header, "Strict-Transport-Security", "max-age=31536000; includeSubDomains" )
            CHAIN( Conn::put_resp_header, "X-Content-Security-Policy", "default-src 'self'; img-src *; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; connect-src 'self' https://fonts.googleapis.com https://fonts.gstatic.com; font-src 'self' https://fonts.googleapis.com https://fonts.gstatic.com; frame-src 'none'" );
    }
}

#endif