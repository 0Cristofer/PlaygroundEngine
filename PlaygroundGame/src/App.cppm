export module PlaygroundGame;

import PlaygroundEngine;
import PlaygroundEngine.App;
import PlaygroundEngine.DebugUi;

import std;

namespace PgG
{
	enum CameraType
	{
		Perspective,
		Orthogonal
	};

	enum class Quality : std::uint8_t
	{
		Low,
		Medium,
		High
	};

	enum class DepthBias : std::int8_t
	{
		Pull = -1,
		Neutral = 0,
		Push = 1
	};

	struct Lens
	{
		[[= PgE::DrawDebug{}]] float FocalLength;
		[[= PgE::DrawDebug{}]] Quality Coating;
	};

	struct Camera
	{
		[[= PgE::DrawDebug{}]] float Position;
		[[= PgE::DrawDebug{}]] bool Enabled;
		[[= PgE::DrawDebug{}]] CameraType Type;
		[[= PgE::DrawDebug{}]] Quality RenderQuality;
		[[= PgE::DrawDebug{}]] DepthBias Bias;
		[[= PgE::DrawDebug{}]] Lens MainLens;
		[[= PgE::DrawDebug{}]] std::string Name;
		[[= PgE::DrawDebug{}]] std::vector<float> Exposures;
		[[= PgE::DrawDebug{}]] Camera* Parent;
		[[= PgE::DrawDebug{}]] const int Serial = 7;
		int Id;
	};

	export class PlaygroundGameAppDescriptor : public PgE::AppDescriptorBase
	{
	public:
		explicit PlaygroundGameAppDescriptor(const PgE::CommandLine commandLine) : AppDescriptorBase(commandLine)
		{}

		std::unique_ptr<PgE::AppBase> GetApp() override;
	};

	export class App : public PgE::AppBase
	{
	public:
		void OnBooted(PgE::EngineContext& engine) override;
		void OnStartRun(PgE::World* world) override;
		void OnStep() override;

	private:
		Camera _camera = {};
	};
}
