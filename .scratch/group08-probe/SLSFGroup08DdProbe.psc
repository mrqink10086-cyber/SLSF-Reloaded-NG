Scriptname SLSFGroup08DdProbe

bool Function Probe(Actor actorRef)
    Armor[] worn = zadNativeFunctions.GetDevices(actorRef, 0, true)
    if worn == None
        return false
    endif

    int index = 0
    while index < worn.Length
        if worn[index] != None
            if zadNativeFunctions.FormHasKeywordString(worn[index] as Form, "zad_DeviousBelt")
                return true
            endif
            if zadNativeFunctions.FormHasKeywordString(worn[index] as Form, "zad_DeviousGag")
                return true
            endif
        endif
        index += 1
    endwhile
    return false
EndFunction
