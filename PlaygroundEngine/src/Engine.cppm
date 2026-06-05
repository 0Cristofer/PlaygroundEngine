export module PlaygroundEngine;

export import PlaygroundEngine.World;
export import PlaygroundEngine.GameObject;

import PlaygroundEngine.App;

import std;

namespace PlaygroundEngine
{
    export class Engine;
    
    export class CommandLine
    {
    public:
        CommandLine(int argc, char** argv) : Argc(argc), Argv(argv) {};
        ~CommandLine() = default;
        
        int Argc;
        char** Argv;
    };

    export class AppDescriptorBase
    {
    public:
        AppDescriptorBase(CommandLine* commandLine) : _commandLine(std::unique_ptr<CommandLine>(commandLine)) {}
        
        virtual std::unique_ptr<Engine> GetEngine(AppDescriptorBase* appDescriptor);
        virtual std::unique_ptr<AppBase> GetApp() = 0;
        
        std::unique_ptr<CommandLine> GetCommandLine();
        
    protected:
        ~AppDescriptorBase() = default;
        
    private:
        std::unique_ptr<CommandLine> _commandLine;
    };
    
    export class Engine
    {
    public:
        Engine(AppDescriptorBase* appDescriptor);

        [[nodiscard]] World* GetWorld() const;
        void Run();
        
    private:
        std::unique_ptr<AppBase> _app;
        std::unique_ptr<World> _currentWorld;
    };
}
