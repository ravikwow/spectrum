    #include "mrim.h"
     
    MRIMProtocol::MRIMProtocol() {
            m_transportFeatures.push_back("jabber:iq:register");
            m_transportFeatures.push_back("jabber:iq:gateway");
            m_transportFeatures.push_back("http://jabber.org/protocol/disco#info");
            m_transportFeatures.push_back("http://jabber.org/protocol/caps");
            m_transportFeatures.push_back("http://jabber.org/protocol/chatstates");
    //      m_transportFeatures.push_back("http://jabber.org/protocol/activity+notify");
            m_transportFeatures.push_back("http://jabber.org/protocol/commands");
     
            m_buddyFeatures.push_back("http://jabber.org/protocol/disco#info");
            m_buddyFeatures.push_back("http://jabber.org/protocol/caps");
            m_buddyFeatures.push_back("http://jabber.org/protocol/chatstates");
            m_buddyFeatures.push_back("http://jabber.org/protocol/commands");
     
    //      m_buddyFeatures.push_back("http://jabber.org/protocol/si/profile/file-transfer");
    //      m_buddyFeatures.push_back("http://jabber.org/protocol/bytestreams");
    //      m_buddyFeatures.push_back("http://jabber.org/protocol/si");
    }
     
    MRIMProtocol::~MRIMProtocol() {}
     
    std::list<std::string> MRIMProtocol::transportFeatures(){
            return m_transportFeatures;
    }
     
    std::list<std::string> MRIMProtocol::buddyFeatures(){
            return m_buddyFeatures;
    }
    Tag *MRIMProtocol::getVCardTag(AbstractUser *user, GList *vcardEntries) {
        PurpleNotifyUserInfoEntry *vcardEntry;
        std::string label;
        std::string firstName;
        std::string lastName;
        std::string header;
     
        Tag *N = new Tag("N");
        Tag *head = new Tag("ADR");
        Tag *vcard = new Tag( "vCard" );
        vcard->addAttribute( "xmlns", "vcard-temp" );
     
        while (vcardEntries) {
            vcardEntry = (PurpleNotifyUserInfoEntry *)(vcardEntries->data);
            if (purple_notify_user_info_entry_get_label(vcardEntry) && purple_notify_user_info_entry_get_value(vcardEntry)){
                label=(std::string)purple_notify_user_info_entry_get_label(vcardEntry);
                Log("vcard label", label << " => " << (std::string)purple_notify_user_info_entry_get_value(vcardEntry));
                if (label=="Firstname"){
                    N->addChild( new Tag("GIVEN", (std::string)purple_notify_user_info_entry_get_value(vcardEntry)));
                    firstName = (std::string)purple_notify_user_info_entry_get_value(vcardEntry);
                }
                else if (label=="Lastname"){
                    N->addChild( new Tag("FAMILY", (std::string)purple_notify_user_info_entry_get_value(vcardEntry)));
                    lastName = (std::string)purple_notify_user_info_entry_get_value(vcardEntry);
                }
                else if (label=="Nickname"){
                    vcard->addChild( new Tag("NICKNAME", (std::string)purple_notify_user_info_entry_get_value(vcardEntry)));
                }
                else if (label=="Sex"){
                    vcard->addChild( new Tag("GENDER", (std::string)purple_notify_user_info_entry_get_value(vcardEntry)));
                }
                else if (label=="Birthday"){
                    vcard->addChild( new Tag("BDAY", (std::string)purple_notify_user_info_entry_get_value(vcardEntry)));
                }
               else if (label=="E-Mail"){
                   vcard->addChild( new Tag("UID", (std::string)purple_notify_user_info_entry_get_value(vcardEntry)));
               }
            }
            vcardEntries = vcardEntries->next;
        }
        if (!firstName.empty() || !lastName.empty())
            vcard->addChild( new Tag("FN", firstName + " " + lastName));
            // add photo ant N if any
            if(!N->children().empty())
                vcard->addChild(N);
     
        return vcard;
    }
     
    std::string MRIMProtocol::text(const std::string &key) {
            if (key == "instructions")
                    return _("Enter your screenname and password:");
            else if (key == "username")
                    return _("Screenname");
            return "not defined";
    }
     
    SPECTRUM_PROTOCOL(mrim, MRIMProtocol)

