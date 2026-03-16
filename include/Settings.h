#pragma once

namespace AMF
{
	class AMFSettings : public REX::Singleton<AMFSettings>
	{
	public:
		void LoadSettings();

		bool enablePitchTranslationFix{ true };
		bool disablePlayerRotationMagnetism{ true };
		bool disablePlayerMovementMagnetism{ true };
		bool disableNpcMovementMagnetism{ true };  // crash with this = true for some people
	};
}