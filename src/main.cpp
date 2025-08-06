#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/LoadingLayer.hpp>

using namespace geode::prelude;

bool canDisableMod(Mod *ModItself)
{
	return ModItself->getID() == "omgrod.geodify" || (!ModItself->needsEarlyLoad() && Mod::get() != ModItself && ModItself->isEnabled());
};
static matjson::Value mods = matjson::makeObject({});

void SaveMods()
{
	Mod::get()->setSavedValue("ModsEnabled", mods);
};

class CCModObject : public CCObject
{
public:
	static CCModObject *create(geode::Mod *mod)
	{
		CCModObject *Node = new CCModObject();
		if (Node)
		{
			Node->m_mod = mod;
			Node->autorelease();
			return Node;
		}
		CC_SAFE_DELETE(Node);
		return nullptr;
	};

	geode::Mod *m_mod;
};

class AttachedFun : public CCNode
{
	void update(bool toggled, geode::Mod *CurrentMod)
	{
		if (!mods.contains(CurrentMod->getID()))
		{
			mods.set(CurrentMod->getID(), toggled);
		}
		else
		{
			mods[CurrentMod->getID()] = toggled;
		}

		if (toggled)
		{
			for (geode::Hook *hook : CurrentMod->getHooks())
			{
				(void)hook->enable();
			}
			if (Loader::get()->isPatchless())
				return;
			for (geode::Patch *Patch : CurrentMod->getPatches())
			{
				(void)Patch->enable();
			}
		}
		else
		{
			for (geode::Hook *hook : CurrentMod->getHooks())
			{
				(void)hook->disable();
			}
			if (Loader::get()->isPatchless())
				return;
			for (geode::Patch *Patch : CurrentMod->getPatches())
			{
				(void)Patch->disable();
			}
		}
	}

public:
	void onSelect(bool toggled, geode::Mod *mod)
	{
		update(toggled, mod);
	}
	void onSelect(CCObject *sender)
	{
		if (sender)
		{
			CCMenuItemToggler *CCSender = static_cast<CCMenuItemToggler *>(sender);
			CCSender->toggle(!CCSender->m_toggled);
			update(CCSender->m_toggled, typeinfo_cast<CCModObject *>(CCSender->getUserObject("mod"_spr))->m_mod);
			SaveMods();
		}
	};
	static AttachedFun *create()
	{
		AttachedFun *Node = new AttachedFun();
		if (Node && Node->init())
		{
			Node->autorelease();
			return Node;
		}
		CC_SAFE_DELETE(Node);
		return nullptr;
	};
};

void loadData()
{
	mods = Mod::get()->getSavedValue<matjson::Value>("ModsEnabled");
	AttachedFun *attached = AttachedFun::create();
	if (!attached)
		return;

	std::for_each(mods.begin(), mods.end(), [&](auto pair)
				  {
			std::string id = pair.getKey().value();
			auto enabled = pair.asBool().unwrapOr(false);

    if (!Loader::get()->isModLoaded(id))
        return;

    attached->onSelect(enabled, Loader::get()->getLoadedMod(id)); });
	attached->release();
};

void showWarning()
{
	Loader::get()->queueInMainThread([=]
									 { geode::createQuickPopup(
										   "Soft loading",
										   "This mod requires <cr>all</c> mods that do not have early load to enable themself on <cy>boot</c> to <cr>function</c> properly\ndo you want to enable them?",
										   "No", "Yes",
										   [=](auto, bool btn2)
										   {
											   if (btn2)
											   {
												   auto allMods = Loader::get()->getAllMods();
												   std::for_each(allMods.begin(), allMods.end(), [&](auto &item)
																 {
								if (!item->shouldLoad() && !item->needsEarlyLoad()) {
									(void)item->enable();
									mods[item->getID()] = false;
								} 	SaveMods(); });
											   }
										   }); });
};

class $modify(MenuLayer)
{

	static void onModify(auto &self)
	{
		(void)self.setHookPriority("MenuLayer::init", INT_MIN / 2 - 1);
	}

	bool init()
	{
		if (!MenuLayer::init())
			return false;

		if (!Mod::get()->setSavedValue("shown-warning-prompt", true))
			showWarning();

		return true;
	}
};

struct InjectDeloading : Modify<InjectDeloading, LoadingLayer>
{
	static void onModify(auto &self)
	{
		if (!self.setHookPriority("LoadingLayer::loadAssets", Priority::FirstPost))
		{
			log::warn("Failed to set LoadingLayer::loadAssets");
		}
	}
	void loadAssets()
	{
		if (!m_fromRefresh)
		{
			loadData();
		};
		LoadingLayer::loadAssets();
	}
};

$execute
{
	new EventListener<EventFilter<ModItemUIEvent>>(+[](ModItemUIEvent *event)
												   {
													   CCNode *item = event->getItem();
													   if (!item)
														   return ListenerResult::Propagate;
													   std::optional<geode::Mod *> getmod = event->getMod();
													   if (!getmod.has_value())
														   return ListenerResult::Propagate;
													   if (!canDisableMod(getmod.value()))
														   return ListenerResult::Propagate;
													   if (CCNode *toggled = item->querySelector("view-menu > enable-toggler"))
													   {

														   AttachedFun *Fun = AttachedFun::create();
														   toggled->addChild(Fun);
														   toggled->setUserObject("mod"_spr, CCModObject::create(getmod.value()));
														   CCMenuItemToggler *tog = typeinfo_cast<CCMenuItemToggler *>(toggled);
														   if (tog)
														   {

															   if (mods.contains(getmod.value()->getID()))
															   {
																   if (mods[getmod.value()->getID()].asBool().unwrapOr(true))
																   {
																	   tog->m_onButton->setVisible(true);
																	   tog->m_offButton->setVisible(false);
																   }
																   else
																   {
																	   tog->m_onButton->setVisible(false);
																	   tog->m_offButton->setVisible(true);
																   }
															   }
															   if (item->getUserObject("Modified"_spr))
																   return ListenerResult::Propagate;
															   tog->setTarget(tog, menu_selector(AttachedFun::onSelect));
															   item->setUserObject("Modified"_spr, CCBool::create(true));
														   }
													   };
													   return ListenerResult::Propagate;
												   });
}
