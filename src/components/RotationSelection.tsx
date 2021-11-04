import { nanoid } from "nanoid";
import { useDispatch, useSelector } from "react-redux"
import { getBaseWowheadUrl } from "../Common";
import { Rotation } from "../data/Rotation"
import i18n from "../i18n/config";
import { toggleRotationSpellSelection } from "../redux/PlayerSlice";
import { RootState } from "../redux/Store"

export default function RotationSelection() {
  const playerStore = useSelector((state: RootState) => state.player);
  const dispatch = useDispatch();

  return(
    <section id="rotation-section">
      <h3>Rotation</h3>
      <ul id="rotation-list">
        {
          Rotation.map(rotationGroup =>
            <div key={nanoid()} style={{display: rotationGroup.varName !== 'curse' && playerStore.settings.rotationOption === 'simChooses' ? 'none' : ''}}>
              <h4>{rotationGroup.header}</h4>
              {
                rotationGroup.spells.map(spell =>
                  <li
                    key={nanoid()}
                    data-checked={(playerStore.rotation[rotationGroup.varName] && playerStore.rotation[rotationGroup.varName][spell.varName] === true) || false}
                    onClick={(e) => { dispatch(toggleRotationSpellSelection({rotationGroup: rotationGroup, spell: spell})); e.preventDefault(); }}>
                    <a href={`${getBaseWowheadUrl(i18n.language)}/spell=${spell.id}`}>
                      <img src={`${process.env.PUBLIC_URL}/img/${spell.iconName}.jpg`} alt={spell.name} />
                    </a>
                  </li>
                )
              }
            </div>
          )
        }
      </ul>
    </section>
  )
}